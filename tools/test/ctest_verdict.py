#!/usr/bin/env python3
"""ctest_verdict.py: reads one `ctest -V` log and says what the run really did.

CTest counts a doctest child that ran no test case, or that returned early after printing
"skipping:", as passed. This reads the log per test and keeps the classes apart:

  executed   passed and (a doctest child with >= 1 test case, or a non-doctest test)
  empty      a doctest child whose summary says "test cases: 0" (a name CTest split, a
             filter that matches nothing): passed in CTest, ran nothing
  skipped    passed after printing "skipping..." (early return), by class:
               required-asset  needs ANNO1503_GAME_DIR (or any original file) and it was absent
               platform        a capability the host lacks (WSL, Windows-only)
               opt-in          an environment switch that was not set (ANNO_SIM_TRANSPORT_STATS=1)
               other           anything else
  failed     CTest status other than Passed

A doctest "N skipped" is the filter excluding the other cases of the binary: ordinary, never
counted here. Exit status: 0 only when nothing failed, something was selected, nothing is empty,
no sanitizer report is in the log and no required-asset skip happened. Platform and opt-in
skips do not fail the gate; they are reported on their own line and are not executed passes.
"""
import re
import sys

SANITIZER = re.compile(r"AddressSanitizer|LeakSanitizer|ThreadSanitizer|runtime error:|UndefinedBehaviorSanitizer")
RESULT = re.compile(r"^\s*\d+/\d+ Test\s+#(\d+): (.*?) \.+\s*(\*\*\*)?\s*(Passed|Failed|Exception|Timeout|Not Run|"
                    r"SEGFAULT|Interrupted|Child aborted|Bus error|Illegal|Other)[^\n]*$")
PREFIX = re.compile(r"^(\d+): ?(.*)$")
DOCTEST = re.compile(r"\[doctest\] test cases:\s*(\d+)\s*\|")
SKIP = re.compile(r"skipping", re.I)


def classify(line):
    l = line.lower()
    if "anno1503_game_dir" in l or "original" in l and "files" in l:
        return "required-asset"
    if "wsl" in l or "skipping on windows" in l:
        return "platform"
    if "set anno_" in l or "opt-in" in l:
        return "opt-in"
    return "other"


def main(path):
    tests = {}  # number -> dict
    sanitizer = 0
    # Only the selection counts: a runner writes "== selection" after its preflight, whose own
    # test must not make an empty selection look selected.
    lines = open(path, errors="replace").read().split("\n")
    if "== selection" in lines:
        lines = lines[lines.index("== selection") + 1:]
    for raw in lines:
        if SANITIZER.search(raw):
            sanitizer += 1
        m = RESULT.match(raw)
        if m:
            t = tests.setdefault(int(m.group(1)), {"cases": None, "skips": []})
            t["name"] = m.group(2)
            t["status"] = m.group(4)
            continue
        m = PREFIX.match(raw)
        if not m:
            continue
        t = tests.setdefault(int(m.group(1)), {"cases": None, "skips": []})
        body = m.group(2)
        d = DOCTEST.search(body)
        if d:
            t["cases"] = int(d.group(1))
        if SKIP.search(body) and "MESSAGE" in body:
            t["skips"].append(classify(body))
    selected = [t for t in tests.values() if "status" in t]
    failed = [t for t in selected if t["status"] != "Passed"]
    passed = [t for t in selected if t["status"] == "Passed"]
    empty = [t for t in passed if t["cases"] == 0]
    skipped = [t for t in passed if t["skips"] and t["cases"] != 0 or (t["skips"] and t["cases"] is None)]
    executed = [t for t in passed if t not in empty and t not in skipped]
    by = {"required-asset": 0, "platform": 0, "opt-in": 0, "other": 0}
    for t in skipped:
        by[t["skips"][0]] += 1
    for t in empty:
        print("EMPTY:", t["name"])
    for t in skipped:
        print("SKIPPED(%s):" % t["skips"][0], t["name"])
    for t in failed:
        print("FAILED:", t["name"], t["status"])
    print("COUNTS: selected=%d executed=%d failed=%d empty=%d skipped=%d (required-asset=%d platform=%d "
          "opt-in=%d other=%d) sanitizer_lines=%d" % (len(selected), len(executed), len(failed), len(empty),
                                                       len(skipped), by["required-asset"], by["platform"],
                                                       by["opt-in"], by["other"], sanitizer))
    if not selected:
        print("VERDICT: FAIL zero selected tests")
        return 8
    if failed:
        print("VERDICT: FAIL failed tests")
        return 1
    if sanitizer:
        print("VERDICT: FAIL sanitizer report in the log")
        return 4
    if by["required-asset"]:
        print("VERDICT: FAIL required asset tests skipped (a prerequisite was absent)")
        return 6
    if empty:
        print("VERDICT: FAIL doctest children executed zero cases")
        return 7
    print("VERDICT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
