#!/usr/bin/env python3
"""window_table.py: the report tables of a window matrix (tools/bench/window_case.sh), from raw files.

    tools/bench/window_table.py <bench_picture binary> <matrix out_dir>

The inventory first (round ed): plan.tsv (written by window_matrix.sh before its first window) lists
every case of the matrix, whether it was selected and its trace mode; results.tsv lists the cases
that ran. Refused: a missing or malformed plan.tsv or results.tsv, a plan that selects no case (exit
2), a case listed twice, a result for a case the plan does not select, results in another order than
the plan's, a selected case without a result (an unfinished campaign), a case directory that
results.tsv does not list, a results status that is not run.txt's window_exit, a run.txt trace mode
that is not the plan's, a trace-off case with a trace directory (exit 1). The table then says whether
it is the full matrix or a requested subset of it. The bench_picture argument is resolved once (a
bare name on PATH), and the file run is the file hashed; cases captured with another bench_picture
(run.txt's hash) are named.

For every case of results.tsv (in its order), with the trace on:
  - the evidence is validated first: the raw trace is aggregated now by the given bench_picture
    (--aggregate events.tsv; 0 complete and successful, 6 failed; incomplete, contradictory or
    anything else is unusable). The aggregate.txt saved at the run is not trusted: a case whose
    saved summary differs from the current one is named (the current one is used). Then frames.tsv
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
case usable and successful, and every selected case ran; 1 some case's evidence is not usable (an
incomplete trace included) or the inventory is not; 6 all usable but some run failed; 2 bad
arguments, no results.tsv or plan.tsv, or no case selected.
"""
import hashlib
import importlib.util
import math
import os
import shutil
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
        # The viewer reconfigures on any change of size: a new drawable is a new epoch.
        if rows and row["epoch"] == rows[-1]["epoch"] and row["drawable"] != rows[-1]["drawable"]:
            raise Unusable(f"{where}: drawable {row['drawable']} in epoch {row['epoch']} of "
                           f"{rows[-1]['drawable']}")
        if rows and row["generation"] < rows[-1]["generation"]:
            raise Unusable(f"{where}: generation {row['generation']} after generation "
                           f"{rows[-1]['generation']}")
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
        if (r["reasons"] == "-") != (row["rejected"] == 0) or (
                row["rejected"] > 0 and len(r["reasons"].split(",")) != row["rejected"]):
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


def load_picture_table():
    """picture_table.py beside this file: its aggregate() (loaded by path, so -I works)."""
    spec = importlib.util.spec_from_file_location("picture_table",
                                                  Path(__file__).with_name("picture_table.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


PICTURE_TABLE = load_picture_table()


def aggregate_lines(binary, case):
    """The current aggregate of the raw trace, by key, and whether the run succeeded."""
    try:
        lines, succeeded = PICTURE_TABLE.aggregate(binary, case / "trace" / "events.tsv")
    except PICTURE_TABLE.Unusable as e:
        raise Unusable(str(e)) from None
    return {l.split(" ", 1)[0].split("=", 1)[0]: l for l in lines}, lines, succeeded


class Inventory(Exception):
    """The matrix's inventory is not usable; `code` is the exit status."""

    def __init__(self, message, code):
        super().__init__(message)
        self.code = code


def read_tsv(path, header):
    try:
        lines = path.read_text().split("\n")
    except OSError as e:
        raise Inventory(f"{path}: {e.strerror or e}", 2) from None
    if lines and lines[-1] == "":
        lines.pop()
    if not lines or lines[0].split("\t") != header:
        raise Inventory(f"{path.name} line 1: not the header {chr(9).join(header)!r}", 1)
    rows = []
    for n, line in enumerate(lines[1:], start=2):
        f = line.split("\t")
        if len(f) != len(header) or not f[0] or "/" in f[0] or f[0] in (".", ".."):
            raise Inventory(f"{path.name} line {n}: not a row of {len(header)} fields with a case "
                            "name", 1)
        rows.append(f)
    return rows


def inventory(matrix):
    """The cases to tabulate (results.tsv order), their statuses and planned trace modes, and the
    campaign line; Inventory when unusable."""
    plan = read_tsv(matrix / "plan.tsv", ["case", "selected", "trace"])
    planned, selected, trace = [], [], {}
    for name, sel, mode in plan:
        if name in planned:
            raise Inventory(f"plan.tsv lists case {name} twice", 1)
        if sel not in ("0", "1"):
            raise Inventory(f"plan.tsv: case {name} selected '{sel}' (0 or 1)", 1)
        if mode not in ("on", "off"):
            raise Inventory(f"plan.tsv: case {name} trace '{mode}' (on or off)", 1)
        planned.append(name)
        trace[name] = mode
        if sel == "1":
            selected.append(name)
    if not selected:
        raise Inventory("plan.tsv selects no case: there is no campaign to tabulate", 2)
    results = read_tsv(matrix / "results.tsv", ["case", "status", "wall_s", "load_after"])
    names = []
    for name, status, _, _ in results:
        if name in names:
            raise Inventory(f"results.tsv lists case {name} twice", 1)
        if name not in selected:
            raise Inventory(f"results.tsv lists case {name}, which plan.tsv does not select", 1)
        if not WHOLE.fullmatch(status):
            raise Inventory(f"results.tsv: case {name} status '{status}' is not a whole number", 1)
        names.append(name)
    if names != [n for n in selected if n in names]:
        raise Inventory("results.tsv lists the cases in another order than plan.tsv", 1)
    # A selected case without a result is an unfinished campaign, whether or not its directory was
    # begun (it was running when the campaign stopped); any other unlisted directory is lost.
    missing = [n for n in selected if n not in names]
    on_disk = sorted(p.name for p in matrix.iterdir() if p.is_dir())
    if missing:
        begun = [n for n in missing if n in on_disk]
        raise Inventory(f"INCOMPLETE campaign: {len(names)} of {len(selected)} selected cases ran; "
                        f"no result for {', '.join(missing)}"
                        + (f" (begun, not finished: {', '.join(begun)})" if begun else ""), 1)
    lost = [d for d in on_disk if d not in names]
    if lost:
        raise Inventory(f"case directory {lost[0]} is not listed in results.tsv", 1)
    statuses = {name: status for name, status, _, _ in results}
    modes = {name: trace[name] for name in names}
    if len(selected) == len(planned):
        campaign = f"Campaign: the full matrix, {len(planned)} of {len(planned)} planned cases."
    else:
        campaign = (f"Campaign: a requested subset, {len(selected)} of {len(planned)} planned cases "
                    f"({', '.join(selected)}); not the full matrix.")
    return names, statuses, modes, campaign


def sha256(path):
    try:
        return hashlib.sha256(Path(path).read_bytes()).hexdigest()
    except OSError as e:
        return f"unreadable ({e.strerror or e})"


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
        if "maximum resident set size" in line:  # macOS /usr/bin/time -l: bytes
            return f"{int(line.split()[0]) / 2**30:.2f} GiB"
        if "Maximum resident set size (kbytes):" in line:  # GNU time -v: KiB
            return f"{int(line.split(':')[1]) / 2**20:.2f} GiB"
        if line.startswith("Peak working set size (bytes):"):  # window_case.py (Windows): bytes
            return f"{int(line.split(':')[1]) / 2**30:.2f} GiB"
    raise Unusable("time.txt has no maximum resident set size")


def main():
    # The table holds non-ASCII text (an arrow); a Windows console or pipe defaults to a legacy code page and
    # the run would die on the first such line (W4-WIN-4: UnicodeEncodeError, cp1252).
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    # The aggregator is resolved once, so the file hashed is the file run (a bare name is looked up
    # on PATH, as subprocess would).
    found = sys.argv[1] if "/" in sys.argv[1] else shutil.which(sys.argv[1])
    if found is None or not os.path.isfile(found) or not os.access(found, os.X_OK):
        print(f"window_table: {sys.argv[1]}: not an executable bench_picture", file=sys.stderr)
        return 2
    binary, matrix = str(Path(found).resolve()), Path(sys.argv[2])
    digest = sha256(binary)
    try:
        names, statuses, modes, campaign = inventory(matrix)
    except Inventory as e:
        print(f"window_table: {matrix}: {e}", file=sys.stderr)
        return e.code
    print(campaign)
    print(f"Aggregate: {binary} sha256 {digest} (every trace aggregated now).")
    print()
    identity, timing, edits, control = [], [], [], []
    oversize, displays_seen = [], set()
    unusable, failed, stale, other_bench = [], [], [], []
    for name in names:
        case = matrix / name
        try:
            run_text = read(case / "run.txt")
            run = kv(run_text.replace("\n", " "))
            # Several lines carry a sha256: the bench's is on its own line.
            captured = next((kv(l).get("sha256") for l in run_text.splitlines()
                             if l.startswith("bench_picture=")), None)
            if run.get("window_exit") != statuses[name]:
                raise Unusable(f"results.tsv status {statuses[name]}, run.txt window_exit "
                               f"{run.get('window_exit')}")
            # The trace mode is the plan's: a run.txt that says otherwise, or a trace-off case with a
            # trace directory, is not believed (round ed review: trace=off skipped the validation).
            if run.get("trace") != modes[name]:
                raise Unusable(f"run.txt trace={run.get('trace')}, plan.tsv trace={modes[name]}")
            if modes[name] == "off" and (case / "trace").exists():
                raise Unusable("a trace-off case with a trace directory")
            fps, busy_s = stderr_intervals(case)
            control.append(f"| {name} | {run.get('trace', '?')} | {dist(fps, 1)} | {dist(busy_s, 2)} | "
                           f"{peak_rss(case)} |")
            if modes[name] == "off":
                if run.get("window_exit") != "0":
                    failed.append(name)
                continue
            if captured not in (None, digest):
                other_bench.append(name)
            agg, agg_text, succeeded = aggregate_lines(binary, case)
            try:
                saved = (case / "aggregate.txt").read_text().splitlines()
            except OSError:
                saved = None
            if saved != agg_text or run.get("aggregate_exit") != ("0" if succeeded else "6"):
                stale.append(name)
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
            if not succeeded or run.get("window_exit") != "0":
                failed.append(name)
            manifest = read(case / "trace" / "manifest.txt").splitlines()
            adapter = next((l[len("adapter "):] for l in manifest if l.startswith("adapter ")), "?")
            drawables = sorted({f["drawable"] for f in frames})
            # Distinct views (position, group, scale): one, unless the session was resized or input
            # reached the window (a test run must not be moved by the host's pointer).
            # A drawable larger than every display of the host (window_case.py records them): a surface
            # bigger than the screen, not a result on a display of that size (round W4-WIN).
            shown = [tuple(int(v) for v in d.split("x")) for d in run.get("displays", "").split(",")
                     if "x" in d]
            displays_seen.update(run.get("displays", "").split(",") if shown else [])
            for d in drawables:
                dr_w, dr_h = (int(v) for v in d.split("x"))
                if shown and not any(dr_w <= dw and dr_h <= dh for dw, dh in shown):
                    oversize.append(f"{name} ({d})")
            views = len({(f["view_x"], f["view_y"], f["view_w"], f["view_h"], f["group"],
                          f["scale"]) for f in frames})
            identity.append(f"| {name} | {run.get('drawable_asked', '?')} | {', '.join(drawables)} | "
                            f"{views} | "
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
    if stale:
        print(f"Saved aggregate.txt differs from the current aggregate (the current one is used): "
              f"{', '.join(stale)}.")
        print()
    if other_bench:
        print(f"Captured with another bench_picture than the one aggregating now: "
              f"{', '.join(other_bench)}.")
        print()
    print("| case | drawable asked | drawable (px) | views | adapter | frames attempted / presented / "
          "failed | dropped | trace |")
    print("|---|---|---|---:|---|---|---:|---|")
    print("\n".join(identity))
    print()
    if oversize:
        print(f"NOTE: the drawable of {', '.join(oversize)} is larger than every display of the host "
              f"(displays: {', '.join(sorted(displays_seen))}): an oversized-surface result, not a "
              f"result on a display of that size; the cadence is the host display's.")
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
