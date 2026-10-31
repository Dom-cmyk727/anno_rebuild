#!/usr/bin/env python3
"""ctest_verdict.py: reads one `ctest -V` log and says what the run really did.

Ported from the MAP-2 reader (commit 8204b6b, tools/test/ctest_verdict.py) for the W4.3 runner
(round ec), with the rendering suites' prerequisites and a selection inventory added.

CTest counts a doctest child that ran no test case, or that returned early after printing
"skipping:", as passed. This reads the log per test and keeps the classes apart:

  selected   a test CTest started in the selection ("Start N: name")
  executed   passed and (a doctest child with >= 1 test case, or a non-doctest test)
  empty      a doctest child whose summary says "test cases: 0" (a name CTest split, a
             filter that matches nothing): passed in CTest, ran nothing
  skipped    passed after printing a skip message, by class:
               required-asset       an original file was absent (ANNO1503_GAME_DIR unset, no game
                                    files, a scenario not in the game directory)
               required-capability  a capability the local gate needs was absent (no GPU device)
               platform             target hardware or a host the gate does not run on (an RTX
                                    4090, WSL, Windows-only)
               opt-in               an environment switch that was not set
               other                any other explicit early return
  failed     CTest status other than Passed or Skipped (failures, crashes, timeouts, not run)
  no-result  started in the selection but without a terminal result line, or counted by CTest
             (the "k/M" of its result lines, its "out of M" summary) but absent from the log; a
             selection without CTest's summary line is cut (round ec review of A)

CTest's own skip (a test whose SKIP_REGULAR_EXPRESSION matched, "***Skipped") is a skip, classed by
the skip message in its output like a doctest one ("other" when none is found).

A skip is a doctest MESSAGE line that says it skipped (the phrasings in use: "skipping: ...",
"...: skipping X", "...: skipped", "skipped: ..."). Per-item outcomes inside an executed test name no
prerequisite and are not skips. A zero-assertion test is not a skip by itself, and a doctest
"N skipped" is the filter excluding the binary's other cases: never counted here.

Exit status: 0 only when something was selected, every selected test has a terminal result,
nothing failed, nothing is empty, no sanitizer report is in the log, and no required-asset or
required-capability skip happened. Platform, opt-in and other skips do not fail the gate; they are
listed and are not executed passes.
  1 failed  4 sanitizer  6 required prerequisite skipped  7 empty  8 nothing selected  9 no result
"""
import re
import sys

SANITIZER = re.compile(r"AddressSanitizer|LeakSanitizer|ThreadSanitizer|runtime error:|UndefinedBehaviorSanitizer")
RESULT = re.compile(r"^\s*\d+/(\d+) Test\s+#(\d+): (.*?) \.+\s*(\*\*\*)?\s*(Passed|Skipped|Failed|Exception|"
                    r"Timeout|Not Run|SEGFAULT|Interrupted|Child aborted|Bus error|Illegal|Other)[^\n]*$")
SUMMARY = re.compile(r"^\s*\d+% tests passed, \d+ tests? failed out of (\d+)\s*$")
START = re.compile(r"^\s*Start\s+(\d+): (.*)$")
PREFIX = re.compile(r"^(\d+): ?(.*)$")
DOCTEST = re.compile(r"\[doctest\] test cases:\s*(\d+)\s*\|")

REQUIRED = re.compile(r"ANNO1503_GAME_DIR (is )?not set|\bno game files\b|is not in this game directory|"
                      r"\bno scenario files\b|\bno endless scenario\b", re.I)
CAPABILITY = re.compile(r"\bno GPU device\b", re.I)
OPT_IN = re.compile(r"\bset ANNO_[A-Z0-9_]+=")
PLATFORM = re.compile(r"WSL \(Ubuntu\) is not available|\bon Windows\b|POSIX shell|\bno RTX 4090\b", re.I)
SKIP_WORD = re.compile(r"skipp(ing|ed)\b", re.I)
PRIORITY = ["required-asset", "required-capability", "opt-in", "platform", "other"]
FAILING = ("required-asset", "required-capability")


def classify(text):
    """The skip class of one MESSAGE text, or None when it is not a skip."""
    if not SKIP_WORD.search(text):
        return None
    if REQUIRED.search(text):
        return "required-asset"
    if CAPABILITY.search(text):
        return "required-capability"
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

    def test(number):
        return tests.setdefault(number, {"cases": None, "skips": [], "output_skips": [],
                                         "started": False})

    counted = 0  # the selection's size as CTest states it (k/M, "out of M")
    summary = False

    for raw in lines:
        if SANITIZER.search(raw):
            sanitizer += 1
        m = RESULT.match(raw)
        if m:
            counted = max(counted, int(m.group(1)))
            t = test(int(m.group(2)))
            t["name"] = m.group(3)
            t["status"] = m.group(5)
            continue
        m = SUMMARY.match(raw)
        if m:
            counted = max(counted, int(m.group(1)))
            summary = True
            continue
        m = START.match(raw)
        if m:
            t = test(int(m.group(1)))
            t["started"] = True
            t.setdefault("name", m.group(2).strip())
            continue
        m = PREFIX.match(raw)
        if not m:
            continue
        t = test(int(m.group(1)))
        body = m.group(2)
        d = DOCTEST.search(body)
        if d:
            t["cases"] = int(d.group(1))
        text = message_text(body)
        if text is not None:
            kind = classify(text)
            if kind:
                t["skips"].append(kind)
        else:
            kind = classify(body)  # plain output: CTest's own skip names its reason here
            if kind:
                t["output_skips"].append(kind)
    selected = [t for t in tests.values() if t["started"] or "status" in t]
    no_result = [t for t in selected if "status" not in t]
    finished = [t for t in selected if "status" in t]
    failed = [t for t in finished if t["status"] not in ("Passed", "Skipped")]
    passed = [t for t in finished if t["status"] == "Passed"]
    ctest_skipped = [t for t in finished if t["status"] == "Skipped"]
    for t in ctest_skipped:
        t["skips"] = t["skips"] + t["output_skips"] or ["other"]
    empty = [t for t in passed if t["cases"] == 0]
    skipped = [t for t in passed if t["skips"] and t["cases"] != 0] + ctest_skipped
    executed = [t for t in passed if t not in empty and t not in skipped]
    # Tests CTest counted that have no line here, and a selection without CTest's summary: cut.
    missing = max(0, counted - len(selected))
    cut = bool(selected) and not summary
    by = {k: 0 for k in PRIORITY}
    for t in skipped:
        by[min(t["skips"], key=PRIORITY.index)] += 1
    for t in empty:
        out.append("EMPTY: " + t["name"])
    for t in skipped:
        out.append("SKIPPED(%s): %s" % (min(t["skips"], key=PRIORITY.index), t["name"]))
    for t in failed:
        out.append("FAILED: %s %s" % (t["name"], t["status"]))
    for t in no_result:
        out.append("NO RESULT: %s" % t.get("name", "?"))
    if missing:
        out.append("NO RESULT: %d test(s) CTest counted (of %d) have no line in the log" %
                   (missing, counted))
    if cut:
        out.append("NO RESULT: the selection has no CTest summary line (the log was cut)")
    out.append("COUNTS: selected=%d executed=%d failed=%d empty=%d no_result=%d skipped=%d "
               "(required-asset=%d required-capability=%d platform=%d opt-in=%d other=%d) "
               "sanitizer_lines=%d" % (len(selected) + missing, len(executed), len(failed), len(empty),
                                       len(no_result) + missing, len(skipped),
                                       by["required-asset"],
                                       by["required-capability"], by["platform"], by["opt-in"],
                                       by["other"], sanitizer))
    if not selected and not counted:
        out.append("VERDICT: FAIL zero selected tests")
        return 8, out
    if failed:
        out.append("VERDICT: FAIL failed tests")
        return 1, out
    if no_result or missing or cut:
        out.append("VERDICT: FAIL selected tests without a terminal result")
        return 9, out
    if sanitizer:
        out.append("VERDICT: FAIL sanitizer report in the log")
        return 4, out
    if any(by[k] for k in FAILING):
        out.append("VERDICT: FAIL required prerequisite skipped (an asset or a capability was absent)")
        return 6, out
    if empty:
        out.append("VERDICT: FAIL doctest children executed zero cases")
        return 7, out
    out.append("VERDICT: PASS")
    return 0, out


def main(path):
    with open(path, errors="replace") as f:
        code, out = verdict(f.read().split("\n"))
    print("\n".join(out))
    return code


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
