#!/usr/bin/env python3
"""Tabulate the peak memory of a test run made with ANNO_TEST_PEAK_MEMORY=ON (C-3).

    peak_report.py <LastTest.log> [--inventory <ctest --show-only=json-v1 output>]
                   [--threshold-mib 1024] [--jobs 8]
    peak_report.py --selftest

Reads CTest's LastTest.log (every test's output), takes the "[anno-test peak]" line
anno_peak_launcher printed for each test, and whether the test reserved memory (an
"[anno-test memory] admit estimate=<n>" line with n > 0; the largest estimate is shown). Prints:

- a table of every test whose peak exceeds the threshold: peak, reservation, processors;
- the largest sum of unreserved peaks that can run at once under -j <jobs>: a set of unreserved
  tests whose PROCESSORS (from the inventory; 1 without one) add up to at most <jobs>. This is an
  upper bound: CTest's real schedule, test order and RESOURCE_LOCKs are not modelled;
- the counts: tests in the log, tests with a peak line, tests without one (not run through the
  launcher, e.g. Python contracts).
"""
import json
import re
import sys

TEST = re.compile(r"^\d+/\d+ Test: (.+)$")
PEAK = re.compile(r"\[anno-test peak\] peak_bytes=(\d+) source=(\S+) exit=(-?\d+) wall_ms=(\d+)")
ADMIT = re.compile(r"\[anno-test memory\] admit estimate=(\d+)")
MIB = 1024 * 1024


def parse(lines):
    """{test: {"peak": bytes or None, "reserve": max admit estimate (0 = none)}}."""
    tests = {}
    current = None
    for line in lines:
        m = TEST.match(line.rstrip("\r\n"))
        if m:
            current = m.group(1)
            # --repeat runs a test several times: keep the largest numbers.
            tests.setdefault(current, {"peak": None, "reserve": 0})
            continue
        if current is None:
            continue
        p = PEAK.search(line)
        if p:
            peak = int(p.group(1))
            old = tests[current]["peak"]
            tests[current]["peak"] = peak if old is None else max(old, peak)
        a = ADMIT.search(line)
        if a:
            tests[current]["reserve"] = max(tests[current]["reserve"], int(a.group(1)))
    return tests


def processors_of(inventory):
    out = {}
    for test in inventory.get("tests", []):
        procs = 1
        for prop in test.get("properties", []):
            if prop.get("name") == "PROCESSORS":
                procs = int(prop.get("value", 1))
        out[test["name"]] = procs
    return out


def max_concurrent(items, jobs):
    """Largest sum of values over a subset whose weights add up to at most jobs (0/1 knapsack).
    items: [(name, weight, value)]. Returns (sum, names)."""
    best = [(0, [])] * (jobs + 1)
    for name, weight, value in items:
        weight = max(1, min(weight, jobs))  # CTest runs a test wider than -j alone
        for cap in range(jobs, weight - 1, -1):
            cand = best[cap - weight][0] + value
            if cand > best[cap][0]:
                best[cap] = (cand, best[cap - weight][1] + [name])
    return best[jobs]


def report(tests, procs, threshold, jobs):
    out = []
    with_peak = {k: v for k, v in tests.items() if v["peak"] is not None}
    out.append("tests in log: %d, with a peak line: %d, without one: %d"
               % (len(tests), len(with_peak), len(tests) - len(with_peak)))
    big = sorted(((v["peak"], k) for k, v in with_peak.items() if v["peak"] > threshold),
                 reverse=True)
    out.append("")
    out.append("| Test | Peak (MiB) | Reserves (MiB) | Processors |")
    out.append("|---|---|---|---|")
    for peak, name in big:
        res = tests[name]["reserve"]
        out.append("| %s | %.0f | %s | %d |" % (name, peak / MIB,
                                                "%.0f" % (res / MIB) if res else "no",
                                                procs.get(name, 1)))
    if not big:
        out.append("| (none above %.0f MiB) | | | |" % (threshold / MIB))
    unreserved = [(k, procs.get(k, 1), v["peak"]) for k, v in with_peak.items()
                  if not v["reserve"]]
    total, names = max_concurrent(unreserved, jobs)
    out.append("")
    out.append("tests above %.0f MiB: %d (%d of them unreserved)"
               % (threshold / MIB, len(big), sum(1 for _, n in big if not tests[n]["reserve"])))
    out.append("largest sum of unreserved peaks at once under -j %d: %.0f MiB over %d test(s)"
               " (upper bound)" % (jobs, total / MIB, len(names)))
    for name in sorted(names, key=lambda n: -tests[n]["peak"])[:10]:
        out.append("  %.0f MiB  %s" % (tests[name]["peak"] / MIB, name))
    return "\n".join(out)


SAMPLE = """1/3 Testing: a.big
1/3 Test: a.big
Output:
----------------------------------------------------------
[anno-test peak] peak_bytes=3221225472 source=ru_maxrss(KiB) exit=0 wall_ms=10 test=big
<end of output>
2/3 Testing: a.reserving
2/3 Test: a.reserving
Output:
----------------------------------------------------------
[anno-test memory] admit estimate=2147483648 available=1 free=1 headroom=1 pending=0
[anno-test peak] peak_bytes=2147483648 source=ru_maxrss(KiB) exit=0 wall_ms=10 test=reserving
<end of output>
3/3 Testing: a.heavy
3/3 Test: a.heavy
Output:
----------------------------------------------------------
[anno-test peak] peak_bytes=2147483648 source=ru_maxrss(KiB) exit=0 wall_ms=10 test=heavy
<end of output>
4/4 Testing: a.python
4/4 Test: a.python
<end of output>
"""


def selftest():
    tests = parse(SAMPLE.splitlines(True))
    ok = True

    def check(cond, what):
        nonlocal ok
        print(("ok: " if cond else "FAIL: ") + what)
        ok = ok and cond

    check(set(tests) == {"a.big", "a.reserving", "a.heavy", "a.python"}, "four tests parsed")
    check(tests["a.big"]["peak"] == 3 * 1024 * MIB, "peak of a.big")
    check(tests["a.reserving"]["reserve"] == 2048 * MIB, "a.reserving reserves")
    check(tests["a.big"]["reserve"] == 0, "a.big reserves nothing")
    check(tests["a.python"]["peak"] is None, "a.python has no peak line")
    # -j 4: heavy takes 4 processors, big 1: either big (3 GiB) alone + nothing, or heavy alone
    # (2 GiB): the bound is 3 GiB, not 5.
    total, names = max_concurrent([("a.big", 1, 3 * 1024 * MIB), ("a.heavy", 4, 2048 * MIB)], 4)
    check(total == 3 * 1024 * MIB and names == ["a.big"], "processors bound the sum (-j 4)")
    total, names = max_concurrent([("a.big", 1, 3 * 1024 * MIB), ("a.heavy", 4, 2048 * MIB)], 5)
    check(total == 5 * 1024 * MIB, "both fit under -j 5")
    text = report(tests, {"a.heavy": 4}, 1024 * MIB, 4)
    check("| a.big | 3072 | no | 1 |" in text, "a.big row")
    check("| a.reserving | 2048 | 2048 | 1 |" in text, "a.reserving row")
    check("3072 MiB over 1 test(s)" in text, "the -j 4 bound line")
    print("RESULT", "PASS" if ok else "FAIL")
    return 0 if ok else 1


def main(argv):
    if argv[1:] == ["--selftest"]:
        return selftest()
    args = argv[1:]
    if not args:
        print(__doc__)
        return 2
    log = args.pop(0)
    inventory, threshold, jobs = None, 1024 * MIB, 8
    while args:
        flag = args.pop(0)
        if flag == "--inventory":
            inventory = args.pop(0)
        elif flag == "--threshold-mib":
            threshold = int(args.pop(0)) * MIB
        elif flag == "--jobs":
            jobs = int(args.pop(0))
        else:
            print("unknown option " + flag)
            return 2
    with open(log, encoding="utf-8", errors="replace") as f:
        tests = parse(f)
    procs = {}
    if inventory:
        with open(inventory, encoding="utf-8") as f:
            procs = processors_of(json.load(f))
    print(report(tests, procs, threshold, jobs))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
