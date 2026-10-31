#!/usr/bin/env python3
"""journal_report.py: read-only summary of a test-budget coordinator journal.

  python3 -I tools/test/journal_report.py <coord-<port>.journal> [...]

Replays A/R (memory holds) and C/D (CPU tokens) the way the coordinator does and prints the
OPEN promises per owner namespace (1 Windows, 2 Linux or an earlier Darwin coordinator, 3 Darwin).
Nothing is written. Used by the rollout procedure (docs/perf/README.md, "MAP-2 repair 3").
"""
import collections
import sys


def report(path):
    holds, cpu = {}, collections.Counter()
    try:
        lines = open(path).read().splitlines()
    except OSError as e:
        print("%s: unreadable (%s)" % (path, e))
        return
    for line in lines:
        f = line.split()
        try:
            if f[0] == "A" and len(f) == 8:
                holds[(f[1], f[2])] = (int(f[3]), int(f[4]), int(f[5]), int(f[6]))
            elif f[0] == "R" and len(f) == 3:
                holds.pop((f[1], f[2]), None)
            elif f[0] == "C" and len(f) == 5:
                cpu[(f[1], f[2], f[3], f[4])] += 1
            elif f[0] == "D" and len(f) == 5 and cpu[(f[1], f[2], f[3], f[4])] > 0:
                cpu[(f[1], f[2], f[3], f[4])] -= 1
        except (IndexError, ValueError):
            pass
    by_ns = collections.defaultdict(lambda: [0, 0, 0])
    for bytes_, ns, pid, _ in ((h[0], h[1], h[2], h[3]) for h in holds.values()):
        by_ns[ns][0] += 1
        by_ns[ns][1] += bytes_
    for (b, ns, pid, st), n in cpu.items():
        by_ns[int(ns)][2] += n
    print("%s: %d open memory holds, %d open CPU tokens" % (path, len(holds), sum(cpu.values())))
    for ns in sorted(by_ns):
        h, b, c = by_ns[ns]
        print("  namespace %d: %d holds (%d bytes), %d tokens" % (ns, h, b, c))


for p in sys.argv[1:]:
    report(p)
