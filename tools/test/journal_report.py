#!/usr/bin/env python3
"""Read-only report of coordinator promises; never a process-liveness verdict.

Unlike the old summary, malformed, truncated or unreadable input exits nonzero.
The reporter conservatively requires complete unsigned decimal records (A/R/C/D),
with field widths matching coord_server.cpp. It does not accept scanf extensions
such as signs or trailing tokens. Exit 0 means parsed, not safe to retire a file.
"""
import collections
import pathlib
import re
import sys


def parse(data):
    holds, cpu = {}, collections.Counter()
    specs = {"A": (64, 64, 64, 32, 64, 64, 32), "R": (64, 64),
             "C": (64, 32, 64, 64), "D": (64, 32, 64, 64)}
    for number, line in enumerate(data.splitlines(keepends=True), 1):
        if not line.strip(b" \t\r\n"):
            continue
        if not line.endswith(b"\n"):
            raise ValueError("line %d is not newline-terminated" % number)
        if len(line) >= 512 or b"\0" in line:
            raise ValueError("line %d is oversized or contains NUL" % number)
        try:
            words = line.decode("ascii").split()
        except UnicodeError as error:
            raise ValueError("line %d is not ASCII" % number) from error
        widths = specs.get(words[0]) if words else None
        if widths is None or len(words) != len(widths) + 1:
            raise ValueError("line %d has an unknown kind or field count" % number)
        values = []
        for word, width in zip(words[1:], widths):
            if not re.fullmatch(r"[0-9]+", word) or int(word) >= 1 << width:
                raise ValueError("line %d has an invalid unsigned field" % number)
            values.append(int(word))
        kind = words[0]
        if kind == "A":
            budget, hold, size, ns, pid, stamp, domain = values
            holds[budget, hold] = (size, ns, pid, stamp, domain)
        elif kind == "R":
            holds.pop(tuple(values), None)
        elif kind == "C":
            cpu[tuple(values)] += 1
        elif cpu[tuple(values)] > 0:
            cpu[tuple(values)] -= 1
    return holds, cpu


def report(path):
    try:
        holds, cpu = parse(pathlib.Path(path).read_bytes())
    except (OSError, ValueError) as error:
        print("%s: UNPROVABLE (%s)" % (path, error), file=sys.stderr)
        return 1
    print("%s: %d open memory holds, %d open CPU tokens" %
          (path, len(holds), sum(cpu.values())))
    for (budget, hold), (size, ns, pid, stamp, domain) in sorted(holds.items()):
        print("  memory budget=%d hold=%d bytes=%d namespace=%d pid=%d started=%d domain=%d" %
              (budget, hold, size, ns, pid, stamp, domain))
    for (budget, ns, pid, stamp), count in sorted(cpu.items()):
        if count:
            print("  cpu budget=%d count=%d namespace=%d pid=%d started=%d" %
                  (budget, count, ns, pid, stamp))
    print("  Parsed records only; owner liveness is unverified. No recovery action authorized.")
    return 0


def main(args):
    if not args:
        print("usage: journal_report.py JOURNAL [JOURNAL ...]", file=sys.stderr)
        return 2
    status = 0
    for path in args:
        status = max(status, report(path))
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
