#!/usr/bin/env python3
"""window_matrix.py: the 13-case window matrix on Windows (the native counterpart of
window_matrix.sh: same plan.tsv / results.tsv, same cases, run one window at a time through
tools/bench/window_case.py).

    python -I tools/bench/window_matrix.py <anno1503.exe> <bench_picture.exe> <new out dir> \
        [<large drawable> [<cases regex>]]

The large drawable defaults to 3840x2160. <cases> is a regular expression that must match a whole
case name (default all). plan.tsv, written before the first window, lists every case and whether it
was selected, so window_table.py can tell a subset from the full matrix. Then:
python -I tools/bench/window_table.py <bench_picture.exe> <out dir>. Needs ANNO1503_GAME_DIR."""
import os
import re
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
W = HERE / "window_workloads"


def main(argv):
    if len(argv) < 3:
        print(__doc__, file=sys.stderr)
        return 2
    app, bench, m = argv[0], argv[1], Path(argv[2])
    large = argv[3] if len(argv) > 3 else "3840x2160"
    select = argv[4] if len(argv) > 4 else ".*"
    if m.exists():
        print(f"window_matrix: {m} exists", file=sys.stderr)
        return 2
    try:
        pat = re.compile(select)
    except re.error:
        print(f"window_matrix: '{select}' is not a regular expression", file=sys.stderr)
        return 2
    cases = []  # name, commands, size, seconds, tile, trace, resize
    for s in ("1920x1080", large):
        cases += [(f"idle-{s}", "idle", s, 12, "1080,444", "on", None),
                  (f"one-{s}", "one", s, 12, "1052,444", "on", None),
                  (f"burst-{s}", "burst", s, 14, "1080,444", "on", None),
                  (f"sustained-{s}", "sustained", s, 32, "1140,444", "on", None)]
    cases.append(("resize-sustained-1920x1080", "sustained", "1920x1080", 32, "1140,444", "on", "12:1600x900"))
    for i in (1, 2):
        cases += [(f"overhead-off-{i}", "sustained", "1920x1080", 32, "1140,444", "off", None),
                  (f"overhead-on-{i}", "sustained", "1920x1080", 32, "1140,444", "on", None)]
    sel = lambda name: pat.fullmatch(name) is not None
    if not any(sel(c[0]) for c in cases):
        print(f"window_matrix: '{select}' selects no case", file=sys.stderr)
        return 2
    m.mkdir(parents=True)
    with open(m / "plan.tsv", "w", newline="\n") as f:
        f.write("case\tselected\ttrace\n")
        for c in cases:
            f.write(f"{c[0]}\t{1 if sel(c[0]) else 0}\t{c[5]}\n")
    with open(m / "results.tsv", "w", newline="\n") as f:
        f.write("case\tstatus\twall_s\tload_after\n")
    for name, commands, size, seconds, at, trace, resize in cases:
        if not sel(name):
            continue
        env = dict(os.environ)
        env.pop("ANNO_VIEW_RESIZE", None)
        if resize:
            env["ANNO_VIEW_RESIZE"] = resize
        t0 = time.time()
        rc = subprocess.run([sys.executable, "-I", str(HERE / "window_case.py"), app, bench, str(m / name),
                             str(W / f"{commands}.commands"), size, str(seconds), at, trace], env=env,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode
        load = "-"
        for line in (m / name / "run.txt").read_text().splitlines() if (m / name / "run.txt").exists() else []:
            if "load_after=" in line:
                load = line.split("load_after=", 1)[1].strip('"')
        with open(m / "results.tsv", "a", newline="\n") as f:
            f.write(f"{name}\t{rc}\t{int(time.time() - t0)}\t{load}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
