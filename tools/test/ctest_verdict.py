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
               gpu             a missing GPU/adapter prerequisite
               other           anything else
  failed     CTest status other than Passed

A skip is a doctest MESSAGE line that names a missing prerequisite AND says it skipped (the
phrasings in use: "skipping: ...", "...: skipping X", "...: skipped", "skipped: ..."). Per-item
outcomes inside an executed test ("overlay-skip ...", "...: skipped as the original does") name no
prerequisite and are not skips. A zero-assertion test is not a skip by itself.

A doctest "N skipped" is the filter excluding the other cases of the binary: ordinary, never
counted here. Exit status: 0 only when nothing failed, something was selected, nothing is empty,
no sanitizer report is in the log and no disallowed prerequisite skip happened. Platform and
opt-in skips are allowed by default; GPU/unknown skips need an explicit policy. Every skip is
reported separately and is not an executed pass. With an inventory, missing terminal results,
unexpected tests and missing doctest summaries also fail. This is a CTest-entry verdict, not
an assertion or nested Python unittest-method coverage report.
"""
import argparse
import json
from pathlib import Path
import re
import sys

SANITIZER = re.compile(r"AddressSanitizer|LeakSanitizer|ThreadSanitizer|runtime error:|UndefinedBehaviorSanitizer")
RESULT = re.compile(r"^\s*\d+/\d+ Test\s+#(\d+): (.*?) \.+\s*(\*\*\*)?\s*(Passed|Failed|Exception|Timeout|Not Run|"
                    r"Skipped|SEGFAULT|Interrupted|Child aborted|Bus error|Illegal|Other)[^\n]*$")
PREFIX = re.compile(r"^(\d+): ?(.*)$")
START = re.compile(r"^\s*Start\s+(\d+): (.*)$")
DOCTEST = re.compile(r"\[doctest\] test cases:\s*(\d+)\s*\|")


REQUIRED = re.compile(r"ANNO1503_GAME_DIR (is )?not set|\bno game files\b|"
                      r"\bno scenario files\b|\bno endless scenario\b|"
                      r"is not in this game directory", re.I)
OPT_IN = re.compile(r"\bset ANNO_[A-Z0-9_]+=")
PLATFORM = re.compile(r"WSL \(Ubuntu\) is not available|\bon Windows\b|POSIX shell|"
                      r"runs only from WSL|which only Linux has|"
                      r"\bWindows[- ]only\b|\bneeds? Windows\b", re.I)
SKIP_WORD = re.compile(r"skipp(ing|ed)\b", re.I)
PRIORITY = ["required-asset", "opt-in", "platform", "gpu", "other"]


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
    if re.search(r"GPU|adapter|RTX|Radeon|Metal|Vulkan", text, re.I):
        return "gpu"
    # Any other explicit early return: the message starts with the skip word.
    if re.match(r"\s*skipp(ing|ed)\b", text, re.I):
        return "other"
    return None


def message_text(body):
    """The text after 'MESSAGE:' of a doctest message line, else None."""
    k = body.find("MESSAGE:")
    return body[k + len("MESSAGE:"):].strip() if k >= 0 else None


def verdict(lines, expected=None, allow_skips=("platform", "opt-in"), require_execution=False):
    """Reads the lines of one log; returns (exit code, output lines)."""
    out = []
    tests = {}  # number -> dict
    sanitizer = 0
    problems = []
    if "== selection" in lines:
        lines = lines[lines.index("== selection") + 1:]
    for raw in lines:
        if SANITIZER.search(raw):
            sanitizer += 1
        m = RESULT.match(raw)
        if m:
            t = tests.setdefault(int(m.group(1)), {"cases": None, "skips": []})
            if "status" in t:
                problems.append("duplicate terminal result: " + m.group(2))
            t["name"] = m.group(2)
            t["status"] = m.group(4)
            continue
        m = START.match(raw)
        if m:
            t = tests.setdefault(int(m.group(1)), {"cases": None, "skips": []})
            t["name"] = m.group(2)
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
        # CMake/CLI contracts do not use doctest MESSAGE. Only accept an explicit
        # prerequisite skip, not an incidental per-item 'skipped' diagnostic.
        if text is None:
            plain = (body[3:] if body.startswith("-- ") else body).strip()
            if re.match(r"skipp(ing|ed)\b", plain, re.I) or (REQUIRED.search(plain) and SKIP_WORD.search(plain)):
                text = plain
        if text is not None:
            kind = classify(text)
            if kind:
                t["skips"].append(kind)
    selected = list(tests.values())
    if expected is not None:
        wanted = {t["name"]: t for t in expected}
        if len(wanted) != len(expected):
            problems.append("duplicate name in selection inventory")
        seen = set()
        for t in selected:
            name = t.get("name")
            if name not in wanted:
                problems.append("unexpected or unnamed test: " + str(name))
            elif name in seen:
                problems.append("duplicate test name: " + name)
            else:
                seen.add(name)
                command = wanted[name].get("command", [])
                if (any(a.startswith("--test-case") for a in command)
                        and t.get("status") == "Passed" and t["cases"] is None):
                    problems.append("missing doctest execution summary: " + name)
        for name in wanted.keys() - seen:
            selected.append({"name": name, "cases": None, "skips": []})
    incomplete = [t for t in selected if "status" not in t]
    failed = [t for t in selected if t.get("status") not in (None, "Passed", "Skipped")]
    passed = [t for t in selected if t.get("status") in ("Passed", "Skipped")]
    for t in passed:
        if t["status"] == "Skipped" and not t["skips"]:
            t["skips"].append("other")
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
    for t in incomplete:
        out.append("INCOMPLETE: " + t.get("name", "unnamed test output"))
    out.extend("INVALID: " + p for p in problems)
    out.append("COUNTS: selected=%d executed=%d failed=%d empty=%d skipped=%d (required-asset=%d platform=%d "
               "opt-in=%d gpu=%d other=%d) incomplete=%d sanitizer_lines=%d" % (len(selected), len(executed), len(failed), len(empty),
                                                           len(skipped), by["required-asset"], by["platform"],
                                                           by["opt-in"], by["gpu"], by["other"], len(incomplete), sanitizer))
    if problems or incomplete:
        out.append("VERDICT: FAIL incomplete or contradictory test evidence")
        return 9, out
    if not selected:
        out.append("VERDICT: FAIL zero selected tests")
        return 8, out
    if failed:
        out.append("VERDICT: FAIL failed tests")
        return 1, out
    if sanitizer:
        out.append("VERDICT: FAIL sanitizer report in the log")
        return 4, out
    if any(by[k] for k in PRIORITY if k not in allow_skips):
        out.append("VERDICT: FAIL prerequisite skip outside the declared gate policy")
        return 6, out
    if empty:
        out.append("VERDICT: FAIL doctest children executed zero cases")
        return 7, out
    if require_execution and not executed:
        out.append("VERDICT: FAIL no selected test executed successfully")
        return 8, out
    out.append("VERDICT: PASS")
    return 0, out


def recorded_verdict(lines, expected=None, allow_skips=("platform", "opt-in")):
    """Classify tests without erasing a recorded gate failure or incomplete closeout."""
    ends = [(i, s[len("GATE_END "):]) for i, s in enumerate(lines) if s.startswith("GATE_END ")]
    canonical = any(s.startswith("== provenance ") for s in lines)
    code, out = verdict(lines, expected, allow_skips, require_execution=canonical or bool(ends))
    if not canonical and not ends:
        return code, out  # Historical log: classification only, not a reconstructed closeout.
    try:
        if len(ends) != 1 or any(s.strip() for s in lines[ends[0][0] + 1:]):
            raise ValueError("missing, repeated or nonterminal GATE_END")
        end = json.loads(ends[0][1])
        status, ctest_rc = end["status"], end["ctest_rc"]
        if type(status) is not int or not 0 <= status <= 255 or (ctest_rc is not None and type(ctest_rc) is not int):
            raise ValueError("invalid gate status")
        if status == 0 and ctest_rc != 0:
            raise ValueError("successful gate contradicts CTest status")
        if status:
            out.append("VERDICT: FAIL recorded gate status=" + str(status))
            return status, out
    except (ValueError, KeyError, TypeError) as e:
        out.append("VERDICT: FAIL gate closeout: " + str(e))
        return 9, out
    return code, out


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("log", type=Path)
    parser.add_argument("--inventory", type=Path)
    parser.add_argument("--allow-skip", action="append", choices=PRIORITY, default=[])
    args = parser.parse_args()
    try:
        expected = json.loads(args.inventory.read_text())["tests"] if args.inventory else None
        code, out = recorded_verdict(args.log.read_text().splitlines(), expected,
                                     {"platform", "opt-in", *args.allow_skip})
    except (OSError, ValueError, KeyError, TypeError) as e:
        print("VERDICT: FAIL unreadable evidence: " + str(e))
        return 9
    print("\n".join(out))
    return code


if __name__ == "__main__":
    sys.exit(main())
