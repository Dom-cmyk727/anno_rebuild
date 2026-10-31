#!/usr/bin/env python3
"""picture_table.py: the report tables of a bench_picture matrix, from its raw files.

    tools/bench/picture_table.py <bench_picture binary> <matrix out_dir>

For every case directory of the matrix (results.tsv order) it recomputes the latency summary from
the case's raw events (bench_picture --aggregate events.tsv), the frame-stage distributions from
frames.tsv (frames of the measured loop only), and reads the memory and verification lines of
summary.txt. Percentiles are nearest-rank with their sample counts; p95 needs 20 samples and p99
100, else the cell says how many there were (docs/perf/README.md, "Picture on a legal city
workload").

Exit status (round ea, 3a/3b): 0 every case's evidence is complete and every run succeeded; 1 some
case's evidence is invalid or incomplete (a missing, unreadable or malformed trace, a trace without
run_end, an aggregate that failed in any other way, a missing or malformed frames.tsv or
summary.txt); 6 all evidence is complete but some run failed; 2 bad arguments or no results.tsv.
Every case is still listed: a case whose evidence is not usable gets a row that says why (and a
line on stderr), and the other cases' rows are printed as usual.
"""
import math
import subprocess
import sys
from pathlib import Path


class Unusable(Exception):
    """A case's evidence cannot be tabulated; the message says why."""


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


def read_lines(path):
    try:
        return path.read_text().splitlines()
    except OSError as e:
        raise Unusable(f"{path.name} unreadable: {e.strerror or e}") from None


def read_frames(path):
    rows = read_lines(path)
    if not rows:
        raise Unusable(f"{path.name} is empty")
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


def one_line(lines, prefix, where):
    found = [l for l in lines if l.startswith(prefix)]
    if not found:
        raise Unusable(f"{where} has no '{prefix.strip()}' line")
    return found[0]


def aggregate(binary, events):
    """The aggregate's lines and whether the run succeeded; Unusable for evidence that is not."""
    try:
        done = subprocess.run([binary, "--aggregate", str(events)], capture_output=True,
                              text=True, check=False)
    except OSError as e:
        raise Unusable(f"aggregate could not run: {e}") from None
    why = done.stderr.strip().replace("\n", " ")
    if done.returncode == 2:
        raise Unusable(f"trace unreadable: {why}")
    lines = done.stdout.splitlines()
    if done.returncode not in (0, 6) or not lines or not lines[0].startswith("run="):
        raise Unusable(f"aggregate failed (exit {done.returncode}): {why or 'no output'}")
    if lines[0].startswith("run=INCOMPLETE"):
        raise Unusable(f"trace incomplete: {lines[0]}")
    if (done.returncode == 0) != lines[0].endswith("(succeeded)"):
        raise Unusable(f"aggregate exit {done.returncode} contradicts '{lines[0]}'")
    return lines, done.returncode == 0


def main():
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    binary, matrix = sys.argv[1], Path(sys.argv[2])
    try:
        results = (matrix / "results.tsv").read_text().splitlines()[1:]
    except OSError as e:
        print(f"picture_table: {matrix / 'results.tsv'}: {e.strerror or e}", file=sys.stderr)
        return 2
    print("| case | status | edits | object-ready ms n / p50 / p95 / p99 / max | visible CPU "
          "chunks ready ms n / p50 / p95 / p99 / max | max age ms | coalesced | offscreen | "
          "unfinished | worker build ms p50 / max |")
    print("|---|---:|---:|---|---|---:|---:|---:|---:|---|")
    stage_rows = []
    memory_rows = []
    progress_rows = []
    unusable = []
    failed = []
    for line in results:
        fields = line.split("\t")
        name = fields[0]
        status = fields[1] if len(fields) > 1 else "?"
        case = matrix / name
        try:
            agg, succeeded = aggregate(binary, case / "events.tsv")
            run = agg[0]
            if not succeeded or status != "0":
                failed.append(name)
                status = f"{status} ({run})"
            agg = agg[1:]
            if not agg:
                raise Unusable("aggregate printed no summary after its run line")
            first = kv(agg[0])
            lines = {l.split()[0]: l for l in agg if l and " n=" in l}

            def cells(key):
                d = kv(lines[key])
                return (f"{d['n']} / {float(d['p50_ms']):.1f} / {d['p95_ms']} / {d['p99_ms']} / "
                        f"{float(d['max_ms']):.1f}")
            age = kv(one_line(agg, "max_age_ms", "the aggregate"))
            build = kv(lines["build_time_worker"])
            row = (f"| {name} | {status} | {first['edits']} | {cells('object_ready')} | "
                   f"{cells('visible_cpu_chunks_ready')} | {float(age['max_age_ms']):.1f} | "
                   f"{first['coalesced']} | {first['offscreen']} | {first['unfinished']} | "
                   f"{float(build['p50_ms']):.1f} / {float(build['max_ms']):.1f} |")
            all_frames = read_frames(case / "frames.tsv")
            # One row per phase, in run order: the measured loop, the drain (a one-edit or burst
            # case commits there), and the pan and zoom of the offscreen case.
            stages = []
            for phase in dict.fromkeys(f["phase"] for f in all_frames):
                frames = [f for f in all_frames if f["phase"] == phase]
                publishing = [f for f in frames if int(f["published"]) > 0]
                committing = [f for f in frames if int(f["committed"]) > 0]
                stages.append(
                    f"| {name} | {phase} | {len(frames)} | "
                    f"{dist([float(f['apply_list_ms']) for f in publishing])} | "
                    f"{dist([float(f['reindex_ms']) for f in publishing])} | "
                    f"{dist([float(f['dispatch_ms']) for f in publishing])} | "
                    f"{dist([float(f['commit_ms']) for f in committing])} | "
                    f"{dist([float(f['update_ms']) for f in frames])} |")
            summary = read_lines(case / "summary.txt")
            mem = kv(one_line(summary, "memory ", "summary.txt"))
            stats = kv(one_line(summary, "stats ", "summary.txt"))
            verdict = [l for l in summary if l.startswith("VERIFY")]
            memory = (
                f"| {name} | {mem['exact_peak_reservation_bytes']} | {stats['result_bytes_peak']} | "
                f"{stats['estimate_shortfalls']} | {mem['exact_peak_incremental_payload']} | "
                f"{mem['sampled_peak_incremental_payload']} | "
                f"{mem['sampled_peak_unreferenced_payload']} | "
                f"{mem['sampled_peak_reclaim_queue_bytes']} | {mem['sampled_peak_rss']} | "
                f"{mem['sampled_peak_footprint']} | {mem['majflt_delta']} | "
                f"{mem['sys_pageouts_delta']} / {mem['sys_swapouts_delta']} | "
                f"{verdict[0] if verdict else 'none'} |")
            progress = (
                f"| {name} | {stats['committed']} | {first['intermediate_commits']} | "
                f"{stats['version_requeues']} | {stats['rebased']} | {stats['discarded']} | "
                f"{stats['coalesced']} | {stats['deferred']} |")
        except Unusable as e:
            unusable.append(name)
            print(f"| {name} | {status} | evidence not usable: {e} |")
            print(f"picture_table: case {name}: evidence not usable: {e}", file=sys.stderr)
            continue
        except (KeyError, IndexError, ValueError) as e:
            unusable.append(name)
            print(f"| {name} | {status} | evidence malformed: {type(e).__name__} {e} |")
            print(f"picture_table: case {name}: evidence malformed: {type(e).__name__} {e}",
                  file=sys.stderr)
            continue
        print(row)
        stage_rows.extend(stages)
        memory_rows.append(memory)
        progress_rows.append(progress)
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
    if not results:
        print("picture_table: results.tsv lists no case", file=sys.stderr)
        return 1
    if unusable:
        print(f"picture_table: {len(unusable)} of {len(results)} case(s) without usable evidence: "
              f"{' '.join(unusable)}", file=sys.stderr)
        return 1
    if failed:
        print(f"picture_table: {len(failed)} of {len(results)} case(s) failed: {' '.join(failed)}",
              file=sys.stderr)
        return 6
    return 0


if __name__ == "__main__":
    sys.exit(main())
