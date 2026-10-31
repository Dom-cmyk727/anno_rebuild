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

A skip is a doctest MESSAGE line that names a missing prerequisite AND says it skipped (the
phrasings in use: "skipping: ...", "...: skipping X", "...: skipped", "skipped: ..."). Per-item
outcomes inside an executed test ("overlay-skip ...", "...: skipped as the original does") name no
prerequisite and are not skips. A zero-assertion test is not a skip by itself.

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


REQUIRED = re.compile(r"ANNO1503_GAME_DIR (is )?not set", re.I)
OPT_IN = re.compile(r"\bset ANNO_[A-Z0-9_]+=")
PLATFORM = re.compile(r"WSL \(Ubuntu\) is not available|\bon Windows\b|POSIX shell", re.I)
SKIP_WORD = re.compile(r"skipp(ing|ed)\b", re.I)
PRIORITY = ["required-asset", "opt-in", "platform", "other"]


def classify(text):
    """The skip class of one MESSAGE text, or None when it is not a skip."""
    if not SKIP_WORD.search(text):
        return None
    if REQUIRED.search(text):
        return "required-asset"
    if OPT_IN.search(text):
        return "opt-in"
    if PLATFORM.search(text):
        return "platform"
    # Any other explicit early return: the message starts with the skip word.
    if re.match(r"\s*skipp(ing|ed)\b", text, re.I):
        return "other"
    return None


def message_text(body):
    """The text after 'MESSAGE:' of a doctest message line, else None."""
    k = body.find("MESSAGE:")
    return body[k + len("MESSAGE:"):].strip() if k >= 0 else None


def verdict(lines):
    """Reads the lines of one log; returns (exit code, output lines)."""
    out = []
    tests = {}  # number -> dict
    sanitizer = 0
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
        text = message_text(body)
        if text is not None:
            kind = classify(text)
            if kind:
                t["skips"].append(kind)
    selected = [t for t in tests.values() if "status" in t]
    failed = [t for t in selected if t["status"] != "Passed"]
    passed = [t for t in selected if t["status"] == "Passed"]
    empty = [t for t in passed if t["cases"] == 0]
    skipped = [t for t in passed if t["skips"] and t["cases"] != 0]
    executed = [t for t in passed if t not in empty and t not in skipped]
    by = {k: 0 for k in PRIORITY}
    for t in skipped:
        by[min(t["skips"], key=PRIORITY.index)] += 1
    for t in empty:
        out.append("EMPTY: " + t["name"])
    for t in skipped:
        out.append("SKIPPED(%s): %s" % (min(t["skips"], key=PRIORITY.index), t["name"]))
    for t in failed:
        out.append("FAILED: %s %s" % (t["name"], t["status"]))
    out.append("COUNTS: selected=%d executed=%d failed=%d empty=%d skipped=%d (required-asset=%d platform=%d "
               "opt-in=%d other=%d) sanitizer_lines=%d" % (len(selected), len(executed), len(failed), len(empty),
                                                           len(skipped), by["required-asset"], by["platform"],
                                                           by["opt-in"], by["other"], sanitizer))
    if not selected:
        out.append("VERDICT: FAIL zero selected tests")
        return 8, out
    if failed:
        out.append("VERDICT: FAIL failed tests")
        return 1, out
    if sanitizer:
        out.append("VERDICT: FAIL sanitizer report in the log")
        return 4, out
    if by["required-asset"]:
        out.append("VERDICT: FAIL required asset tests skipped (a prerequisite was absent)")
        return 6, out
    if empty:
        out.append("VERDICT: FAIL doctest children executed zero cases")
        return 7, out
    out.append("VERDICT: PASS")
    return 0, out


def main(path):
    code, out = verdict(open(path, errors="replace").read().split("\n"))
    print("\n".join(out))
    return code


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
