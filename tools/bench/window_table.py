#!/usr/bin/env python3
"""window_table.py: the report tables of a window matrix (tools/bench/window_case.sh), from raw files.

    tools/bench/window_table.py <matrix out_dir>

For every case of results.tsv (in its order), with the trace on:
  - the evidence is validated first: aggregate_exit in run.txt (bench_picture --aggregate of the
    trace: 0 complete and successful, 6 failed; incomplete, contradictory or anything else is unusable), frames.tsv
    and sim.tsv read strictly (docs/app/window-trace.md: the header, whole tokens, frame ids 1, 2,
    3, ... with no gap, frame starts that never go back, an outcome that matches the acquire and the
    present status, turns that increase), and the files agree with the aggregate (frames submitted,
    presented and failed; batches published);
  - every frame row is cross-checked against the trace's frame events: the same frame ids, epochs
    and times (frame_submitted at submit_ms, frame_presented or frame_failed at the present's end,
    frame_failed at the acquire's end for an attempt without a texture), and an outcome that is the
    viewer's for its acquire status; the commands applied never exceed the command file's;
  - then: the drawable and adapter, frames attempted / presented / failed, the frame-start interval,
    the acquire and present-call durations, the window thread's busy time per presented frame
    (draw start to present return, less the acquire), the commands in the file, applied, and the turns accepted / rejected /
    ambiguous, each batch's publication to its take, the edit boundaries from the aggregate (object-ready, visible CPU-ready,
    GPU-usable, present-call returned; n / p50 / p95 / p99 / max), unfinished work, and the peak
    RSS (/usr/bin/time -l).
With the trace off (the overhead control) only the window's own 2-second stderr lines exist: their
fps and average busy time are listed for every case (the first interval, the start, left out), so
the two can be compared on the same measure.

A present-call return is not display: nothing here observes scanout or GPU completion.
Percentiles are nearest-rank with their sample counts (p95 needs 20, p99 100). Exit status: 0 every
case usable and successful; 1 some case's evidence is not usable (an incomplete trace included); 6 all
usable but some run failed; 2 bad arguments or no results.tsv.
"""
import math
import re
import sys
from pathlib import Path

COLUMNS = ["frame", "epoch", "generation", "view_x", "view_y", "view_w", "view_h", "group", "scale",
           "drawable_w", "drawable_h", "t_start_ms", "picture_ms", "acquire_start_ms",
           "acquire_end_ms", "acquire_status", "texture", "submit_ms", "present_start_ms",
           "present_end_ms", "present_status", "outcome"]
SIM_COLUMNS = ["turn", "t_ms", "applied", "rejected", "outcome", "reasons", "batch", "deltas"]
WHOLE = re.compile(r"-?[0-9]+")
NUMBER = re.compile(r"-?[0-9]+(\.[0-9]+)?([eE][-+]?[0-9]+)?")
ACQUIRE = {"SuccessOptimal", "SuccessSuboptimal", "Timeout", "Outdated", "Lost", "Error", "Occluded"}
OUTCOMES = {"presented", "present_failed", "acquire_skipped", "acquire_retry", "fatal"}
# What the viewer does with each acquire status (classify_acquire), so the outcomes a row may have.
ALLOWED = {"SuccessOptimal": {"presented", "present_failed"},
           "SuccessSuboptimal": {"presented", "present_failed"},
           "Timeout": {"acquire_skipped"}, "Occluded": {"acquire_skipped"},
           "Outdated": {"acquire_retry", "fatal"}, "Lost": {"acquire_retry", "fatal"},
           "Error": {"fatal"}}
PRESENT_STATUS = re.compile(r"-|Success|Error\([0-9]+\)")
FPS = re.compile(r"px: ([0-9.]+) fps, frame avg ([0-9.]+) ms / worst ([0-9.]+) ms")


class Unusable(Exception):
    """A case's evidence cannot be tabulated; the message says why."""


def nearest_rank(v, p):
    return v[max(0, min(len(v) - 1, math.ceil(p * len(v)) - 1))]


def dist(values, digits=2):
    v = sorted(values)
    if not v:
        return "n=0"
    f = f"{{:.{digits}f}}"
    p95 = f.format(nearest_rank(v, 0.95)) if len(v) >= 20 else f"(n={len(v)})"
    p99 = f.format(nearest_rank(v, 0.99)) if len(v) >= 100 else f"(n={len(v)})"
    return f"{len(v)} / {f.format(nearest_rank(v, 0.5))} / {p95} / {p99} / {f.format(v[-1])}"


def read(path):
    try:
        return path.read_text()
    except OSError as e:
        raise Unusable(f"{path.name} unreadable: {e.strerror or e}") from None


def kv(text):
    out = {}
    for tok in text.split():
        if "=" in tok:
            k, v = tok.split("=", 1)
            out[k] = v.strip('"')
    return out


def number(v, where):
    if not NUMBER.fullmatch(v):
        raise Unusable(f"{where}: '{v}' is not a number")
    x = float(v)
    if not math.isfinite(x):
        raise Unusable(f"{where}: '{v}' is not finite")
    return x


def whole(v, where):
    if not WHOLE.fullmatch(v):
        raise Unusable(f"{where}: '{v}' is not a whole number")
    return int(v)


def read_frames(path):
    lines = read(path).split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    if not lines or lines[0].split("\t") != COLUMNS:
        raise Unusable(f"{path.name} line 1: not the window frames header")
    if len(lines) == 1:
        raise Unusable(f"{path.name} has a header and no frame row")
    rows = []
    for n, line in enumerate(lines[1:], start=2):
        f = line.split("\t")
        where = f"{path.name} line {n}"
        if len(f) != len(COLUMNS):
            raise Unusable(f"{where}: {len(f)} fields, expected {len(COLUMNS)}")
        r = dict(zip(COLUMNS, f))
        row = {"frame": whole(r["frame"], where), "epoch": whole(r["epoch"], where),
               "drawable": f"{whole(r['drawable_w'], where)}x{whole(r['drawable_h'], where)}",
               "texture": whole(r["texture"], where), "outcome": r["outcome"],
               "acquire_status": r["acquire_status"], "present_status": r["present_status"],
               "raw": r}
        for c in ("generation", "view_x", "view_y", "view_w", "view_h", "group"):
            row[c] = whole(r[c], where)
        for c in ("t_start_ms", "picture_ms", "acquire_start_ms", "acquire_end_ms", "submit_ms",
                  "present_start_ms", "present_end_ms", "scale"):
            row[c] = number(r[c], where)
        if row["frame"] != len(rows) + 1:
            raise Unusable(f"{where}: frame {row['frame']}, expected {len(rows) + 1} (ids have no "
                           "gap)")
        if rows and row["t_start_ms"] < rows[-1]["t_start_ms"]:
            raise Unusable(f"{where}: t_start_ms goes back")
        if rows and row["epoch"] < rows[-1]["epoch"]:
            raise Unusable(f"{where}: epoch {row['epoch']} after epoch {rows[-1]['epoch']}")
        if rows and row["t_start_ms"] < rows[-1]["end"]:
            raise Unusable(f"{where}: the frame starts before the previous one ended")
        if not (row["t_start_ms"] <= row["acquire_start_ms"] <= row["acquire_end_ms"]):
            raise Unusable(f"{where}: the acquire is out of order")
        if not PRESENT_STATUS.fullmatch(row["present_status"]):
            raise Unusable(f"{where}: present status '{row['present_status']}' is not a status")
        if row["acquire_status"] not in ACQUIRE and not row["acquire_status"].startswith("unknown("):
            raise Unusable(f"{where}: acquire_status '{row['acquire_status']}' is not a status")
        if row["outcome"] not in OUTCOMES:
            raise Unusable(f"{where}: outcome '{row['outcome']}' is not one of the outcomes")
        drawn = row["outcome"] in ("presented", "present_failed")
        texture = row["acquire_status"] in ("SuccessOptimal", "SuccessSuboptimal")
        if row["texture"] != int(texture) or drawn != texture:
            raise Unusable(f"{where}: outcome {row['outcome']} with {row['acquire_status']} and "
                           f"texture {row['texture']}")
        if drawn != (row["present_status"] != "-"):
            raise Unusable(f"{where}: outcome {row['outcome']} with present status "
                           f"{row['present_status']}")
        if (row["outcome"] == "presented") != (row["present_status"] == "Success"):
            raise Unusable(f"{where}: outcome {row['outcome']} with present status "
                           f"{row['present_status']}")
        if drawn and not (row["acquire_start_ms"] <= row["acquire_end_ms"] <= row["submit_ms"] <=
                          row["present_start_ms"] <= row["present_end_ms"]):
            raise Unusable(f"{where}: the frame's boundaries are out of order")
        if not drawn and any(row[c] != -1 for c in ("submit_ms", "present_start_ms",
                                                    "present_end_ms")):
            raise Unusable(f"{where}: an attempt without a texture has submit or present times")
        allowed = ALLOWED.get(row["acquire_status"], {"fatal"})
        if row["outcome"] not in allowed:
            raise Unusable(f"{where}: outcome {row['outcome']} after {row['acquire_status']} (the "
                           f"viewer gives {' or '.join(sorted(allowed))})")
        row["end"] = row["present_end_ms"] if drawn else row["acquire_end_ms"]
        rows.append(row)
    return rows


def read_sim(path):
    lines = read(path).split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    if not lines or lines[0].split("\t") != SIM_COLUMNS:
        raise Unusable(f"{path.name} line 1: not the sim header")
    rows = []
    for n, line in enumerate(lines[1:], start=2):
        f = line.split("\t")
        where = f"{path.name} line {n}"
        if len(f) != len(SIM_COLUMNS):
            raise Unusable(f"{where}: {len(f)} fields, expected {len(SIM_COLUMNS)}")
        r = dict(zip(SIM_COLUMNS, f))
        row = {"turn": whole(r["turn"], where), "applied": whole(r["applied"], where),
               "rejected": whole(r["rejected"], where), "outcome": r["outcome"],
               "batch": whole(r["batch"], where)}
        number(r["t_ms"], where)
        deltas = whole(r["deltas"], where)
        if row["batch"] not in (0, 1) or (row["batch"] == 1) != (deltas > 0):
            raise Unusable(f"{where}: batch {row['batch']} with {deltas} deltas")
        if row["rejected"] > row["applied"]:
            raise Unusable(f"{where}: {row['rejected']} rejections for {row['applied']} commands")
        if (r["reasons"] == "-") != (row["rejected"] == 0):
            raise Unusable(f"{where}: reasons '{r['reasons']}' for {row['rejected']} rejections")
        if rows and row["turn"] <= rows[-1]["turn"]:
            raise Unusable(f"{where}: turn {row['turn']} does not increase")
        a, j = row["applied"], row["rejected"]
        expected = ("none" if a == 0 else "accepted" if j == 0 else
                    "rejected" if j >= a else "ambiguous")
        if row["outcome"] != expected:
            raise Unusable(f"{where}: outcome {row['outcome']} for {a} applied, {j} rejected")
        rows.append(row)
    return rows


def read_frame_events(path):
    """The frame events of a trace the aggregate accepted: kind -> {frame: (epoch, time text)}."""
    lines = read(path).split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    if not lines or lines[0] != "t_ms\tkind\tisland\tversion":
        raise Unusable(f"{path.name} line 1: not the trace header")
    out = {"frame_submitted": {}, "frame_presented": {}, "frame_failed": {}}
    for n, line in enumerate(lines[1:], start=2):
        f = line.split("\t")
        if len(f) != 4:
            raise Unusable(f"{path.name} line {n}: not a trace event")
        if f[1] in out:
            out[f[1]][whole(f[3], f"{path.name} line {n}")] = (
                whole(f[2], path.name), number(f[0], f"{path.name} line {n}"))
    return out


def cross_check(frames, events):
    """Every frame row against its events: the same ids, epochs and times (round ec review)."""
    ids = {f["frame"] for f in frames}
    for kind, by in events.items():
        stray = sorted(set(by) - ids)
        if stray:
            raise Unusable(f"events.tsv has {kind} of frame {stray[0]} with no frames.tsv row")
    for f in frames:
        drawn = f["outcome"] in ("presented", "present_failed")
        expect = {"frame_submitted": (f["epoch"], f["submit_ms"]) if drawn else None,
                  "frame_presented": (f["epoch"], f["present_end_ms"])
                  if f["outcome"] == "presented" else None,
                  "frame_failed": None if f["outcome"] == "presented" else
                  (f["epoch"], f["present_end_ms"] if drawn else f["acquire_end_ms"])}
        for kind, want in expect.items():
            have = events[kind].get(f["frame"])
            if have != want:
                raise Unusable(f"frame {f['frame']}: frames.tsv gives {kind} {want}, events.tsv "
                               f"{have}")


def aggregate_lines(case):
    text = read(case / "aggregate.txt").splitlines()
    if not text or not text[0].startswith("run="):
        raise Unusable("aggregate.txt has no run line")
    return {l.split(" ", 1)[0].split("=", 1)[0]: l for l in text}


def boundary(lines, key):
    if key not in lines:
        return "–"
    d = kv(lines[key])
    if d.get("n", "0") == "0":
        return "n=0"
    p95 = f"{float(d['p95_ms']):.1f}" if d["p95_ms"] != "insufficient" else f"(n={d['n']})"
    p99 = f"{float(d['p99_ms']):.1f}" if d["p99_ms"] != "insufficient" else f"(n={d['n']})"
    return f"{d['n']} / {float(d['p50_ms']):.1f} / {p95} / {p99} / {float(d['max_ms']):.1f}"


def take_ms(case):
    """Per batch, its publication (sim thread) to its take (the window's frame boundary)."""
    published, out = {}, []
    for line in read(case / "trace" / "events.tsv").split("\n")[1:]:
        f = line.split("\t")
        if len(f) == 4 and f[1] == "batch_published":
            published[f[3]] = float(f[0])
        elif len(f) == 4 and f[1] == "batch_taken" and f[3] in published:
            out.append(float(f[0]) - published[f[3]])
    return out


def stderr_intervals(case):
    fps, busy = [], []
    for line in read(case / "stderr.txt").splitlines():
        m = FPS.search(line)
        if m:
            fps.append(float(m.group(1)))
            busy.append(float(m.group(2)))
    return fps[1:], busy[1:]  # the first interval holds the start


def peak_rss(case):
    for line in read(case / "time.txt").splitlines():
        if "maximum resident set size" in line:
            return f"{int(line.split()[0]) / 2**30:.2f} GiB"
    raise Unusable("time.txt has no maximum resident set size")


def main():
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    matrix = Path(sys.argv[1])
    try:
        results = (matrix / "results.tsv").read_text().splitlines()[1:]
    except OSError as e:
        print(f"window_table: {matrix / 'results.tsv'}: {e.strerror or e}", file=sys.stderr)
        return 2
    identity, timing, edits, control = [], [], [], []
    unusable, failed = [], []
    for line in results:
        name = line.split("\t")[0]
        case = matrix / name
        try:
            run = kv(read(case / "run.txt").replace("\n", " "))
            fps, busy_s = stderr_intervals(case)
            control.append(f"| {name} | {run.get('trace', '?')} | {dist(fps, 1)} | {dist(busy_s, 2)} | "
                           f"{peak_rss(case)} |")
            if run.get("trace") == "off":
                if run.get("window_exit") != "0":
                    failed.append(name)
                continue
            agg_exit = run.get("aggregate_exit")
            if agg_exit not in ("0", "6"):
                why = read(case / "aggregate.stderr").strip().replace("\n", " ")
                raise Unusable(f"aggregate exit {agg_exit}: {why or 'no output'}")
            agg = aggregate_lines(case)
            if agg["run"].startswith("run=INCOMPLETE"):
                raise Unusable(f"trace incomplete: {agg['run']}")
            if (agg_exit == "0") != agg["run"].endswith("(succeeded)"):
                raise Unusable(f"aggregate exit {agg_exit} contradicts '{agg['run']}'")
            frames = read_frames(case / "trace" / "frames.tsv")
            sim = read_sim(case / "trace" / "sim.tsv")
            w = kv(agg.get("window", ""))
            submitted = sum(1 for f in frames if f["texture"] == 1)
            presented = sum(1 for f in frames if f["outcome"] == "presented")
            failed_rows = len(frames) - presented
            for key, have in (("frames_submitted", submitted), ("frames_presented", presented),
                              ("frames_failed", failed_rows),
                              ("batches_published", sum(f["batch"] for f in sim))):
                if w.get(key) != str(have):
                    raise Unusable(f"frames.tsv/sim.tsv give {key}={have}, the trace {w.get(key)}")
            cross_check(frames, read_frame_events(case / "trace" / "events.tsv"))
            if agg_exit != "0" or run.get("window_exit") != "0":
                failed.append(name)
            manifest = read(case / "trace" / "manifest.txt").splitlines()
            adapter = next((l[len("adapter "):] for l in manifest if l.startswith("adapter ")), "?")
            drawables = sorted({f["drawable"] for f in frames})
            identity.append(f"| {name} | {run.get('drawable_asked', '?')} | {', '.join(drawables)} | "
                            f"{adapter} | {len(frames)} / {presented} / {failed_rows} | "
                            f"{kv(next((l for l in manifest if l.startswith('trace ')), '')).get('dropped', '?')} | "
                            f"{agg['run']} |")
            starts = [f["t_start_ms"] for f in frames]
            drawn = [f for f in frames if f["outcome"] in ("presented", "present_failed")]
            timing.append(
                f"| {name} | {dist([b - a for a, b in zip(starts, starts[1:])])} | "
                f"{dist([f['acquire_end_ms'] - f['acquire_start_ms'] for f in frames])} | "
                f"{dist([f['present_end_ms'] - f['present_start_ms'] for f in drawn])} | "
                f"{dist([(f['present_end_ms'] - f['t_start_ms']) - (f['acquire_end_ms'] - f['acquire_start_ms']) for f in drawn if f['outcome'] == 'presented'])} |")
            applied = sum(s["applied"] for s in sim)
            in_file = int(run.get("lines", "-1")) if WHOLE.fullmatch(run.get("lines", "")) else -1
            if in_file < 0 or applied > in_file:
                raise Unusable(f"{applied} commands applied for {run.get('lines', '?')} in the "
                               "command file")
            by = {k: sum(1 for s in sim if s["outcome"] == k) for k in ("accepted", "rejected",
                                                                        "ambiguous")}
            e = kv(agg.get("edits", ""))
            g = kv(agg.get("gpu_mode", ""))
            edits.append(
                f"| {name} | {in_file} / {applied} / {by['accepted']} / {by['rejected']} / "
                f"{by['ambiguous']} | {dist(take_ms(case), 1)} | "
                f"{e.get('edits', '?')} | {e.get('offscreen', '?')} | {e.get('coalesced', '?')} | "
                f"{boundary(agg, 'object_ready')} | {boundary(agg, 'visible_cpu_chunks_ready')} | "
                f"{boundary(agg, 'visible_gpu_usable')} | "
                f"{boundary(agg, 'visible_present_call_returned')} | "
                f"{e.get('unfinished', '?')} / {g.get('gpu_unfinished', '?')} / "
                f"{w.get('present_unfinished', '?')} |")
        except Unusable as err:
            print(f"| {name} | evidence not usable: {err} |")
            print(f"window_table: case {name}: evidence not usable: {err}", file=sys.stderr)
            unusable.append(name)
    print("| case | drawable asked | drawable (px) | adapter | frames attempted / presented / failed "
          "| dropped | trace |")
    print("|---|---|---|---|---|---:|---|")
    print("\n".join(identity))
    print()
    print("Per attempted frame, ms, n / p50 / p95 / p99 / max:")
    print()
    print("| case | frame-start interval | acquire (the wait for a surface texture) | present call "
          "| window thread busy per presented frame |")
    print("|---|---|---|---|---|")
    print("\n".join(timing))
    print()
    print("Commands by turn (sim.tsv) and edit boundaries (the aggregate), ms from the edit's "
          "desired event, n / p50 / p95 / p99 / max:")
    print()
    print("| case | commands in the file / applied / turns accepted / rejected / ambiguous | batch "
          "published → taken | edits | offscreen | coalesced | object-ready | visible CPU-ready | "
          "GPU-usable | present call returned | unfinished: object / GPU / present |")
    print("|---|---|---|---:|---:|---:|---|---|---|---|---|")
    print("\n".join(edits))
    print()
    print("The window's own 2-second lines (stderr; the first interval left out), n / p50 / p95 / "
          "p99 / max:")
    print()
    print("| case | trace | fps | average busy ms per frame | peak RSS |")
    print("|---|---|---|---|---:|")
    print("\n".join(control))
    if unusable:
        return 1
    return 6 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
