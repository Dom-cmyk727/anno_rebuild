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
  failed     CTest status other than Passed (excluding refusal-only failures)
  refused    failed solely because the test-support memory coordinator refused admission
             (`[anno-test memory] refused estimate=<n> reason=<reason>`, with no failed
             assertion, crash, timeout, non-refusal exception, or sanitizer report)

A skip is a doctest MESSAGE line that names a missing prerequisite AND says it skipped (the
phrasings in use: "skipping: ...", "...: skipping X", "...: skipped", "skipped: ..."). Per-item
outcomes inside an executed test ("overlay-skip ...", "...: skipped as the original does") name no
prerequisite and are not skips. A zero-assertion test is not a skip by itself.

A doctest "N skipped" is the filter excluding the other cases of the binary: ordinary, never
counted here. Exit status: 0 only when nothing failed, nothing was refused, something was selected,
nothing is empty, no sanitizer report is in the log and no disallowed prerequisite skip happened
(10 when refused tests remain and no higher-precedence failure or sanitizer report occurred).
Platform and opt-in skips are allowed by default; GPU/unknown skips need an explicit policy. Every
skip is reported separately and is not an executed pass. With an inventory, missing terminal
results, unexpected tests and missing doctest summaries also fail. This is a CTest-entry verdict,
not an assertion or nested Python unittest-method coverage report.
"""
import argparse
import json
from pathlib import Path
import re
import sys

SANITIZER = re.compile(r"AddressSanitizer|LeakSanitizer|ThreadSanitizer|runtime error:|UndefinedBehaviorSanitizer")
REFUSAL = re.compile(r"\[anno-test memory\] refused estimate=\d+ reason=\S")
DOCTEST_MARK = re.compile(r"\[doctest\]|test case (?:THREW exception|CRASHED):")
DOCTEST_ASSERTS = re.compile(r"\[doctest\] assertions:\s*\d+\s*\|\s*\d+\s*passed\s*\|\s*(\d+)\s*failed\s*\|")
DOCTEST_THREW = re.compile(r"test case THREW exception:\s*(.*)$")
DOCTEST_REAL_FAIL = re.compile(
    r"FATAL ERROR:|"
    r"ERROR:\s*(?:CHECK|REQUIRE|FAST_CHECK|FAST_REQUIRE|FAIL)\w*\b|"
    r"test case CRASHED:|"
    r"Test case exceeded time limit|"
    r"Should have failed but didn't!|"
    r"Didn't fail exactly|"
    r"Aborting - too many failed asserts!"
)
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


def new_test(name=None):
    t = {"cases": None, "skips": [], "sanitizer": False, "refused": False,
         "real_failure": False, "failed_asserts": 0, "doctest_seen": False,
         "expect_doctest": False}
    if name is not None:
        t["name"] = name
    return t


def is_refused_failure(t):
    """True iff a failed test failed solely due to a test-support admission refusal."""
    if t.get("status") != "Failed":
        return False
    if t["sanitizer"] or t["real_failure"] or t["failed_asserts"] > 0:
        return False
    if (t["doctest_seen"] or t["expect_doctest"]) and (t["cases"] is None or t["cases"] <= 0):
        return False
    return t["refused"]


def format_verdict(selected_count, executed, failed, refused, empty, skipped, incomplete, problems,
                   sanitizer, allow_skips, require_execution, retried_passed=(),
                   counts_label="COUNTS"):
    out = []
    by = {k: 0 for k in PRIORITY}
    for t in skipped:
        by[min(t["skips"], key=PRIORITY.index)] += 1
    for t in empty:
        out.append("EMPTY: " + t["name"])
    for t in skipped:
        out.append("SKIPPED(%s): %s" % (min(t["skips"], key=PRIORITY.index), t["name"]))
    for name in retried_passed:
        out.append("RETRIED: %s passed" % name)
    for t in failed:
        out.append("FAILED: %s %s" % (t["name"], t["status"]))
    for t in refused:
        out.append("REFUSED: " + t["name"])
    for t in incomplete:
        out.append("INCOMPLETE: " + t.get("name", "unnamed test output"))
    out.extend("INVALID: " + p for p in problems)
    out.append("%s: selected=%d executed=%d failed=%d refused=%d empty=%d skipped=%d "
               "(required-asset=%d platform=%d opt-in=%d gpu=%d other=%d) "
               "incomplete=%d sanitizer_lines=%d" % (
                   counts_label, selected_count, len(executed), len(failed), len(refused),
                   len(empty), len(skipped), by["required-asset"], by["platform"],
                   by["opt-in"], by["gpu"], by["other"], len(incomplete), sanitizer))
    if problems or incomplete:
        out.append("VERDICT: FAIL incomplete or contradictory test evidence")
        return 9, out
    if not selected_count:
        out.append("VERDICT: FAIL zero selected tests")
        return 8, out
    if failed:
        out.append("VERDICT: FAIL failed tests")
        return 1, out
    if sanitizer:
        out.append("VERDICT: FAIL sanitizer report in the log")
        return 4, out
    if refused:
        out.append("VERDICT: FAIL refused tests (not run)")
        return 10, out
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


def analyze(lines, expected=None, allow_skips=("platform", "opt-in"), require_execution=False):
    """Parses and classifies one selection (or single retry) log; returns details + formatted verdict."""
    tests = {}  # number -> dict
    sanitizer = 0
    problems = []
    if "== selection" in lines:
        lines = lines[lines.index("== selection") + 1:]
        for idx, raw in enumerate(lines):
            if raw.startswith("== retry refused ") or raw.startswith("== recording_after "):
                lines = lines[:idx]
                break
    for raw in lines:
        has_sanitizer = bool(SANITIZER.search(raw))
        if has_sanitizer:
            sanitizer += 1
        m = RESULT.match(raw)
        if m:
            t = tests.setdefault(int(m.group(1)), new_test())
            if "status" in t:
                problems.append("duplicate terminal result: " + m.group(2))
            t["name"] = m.group(2)
            t["status"] = m.group(4)
            continue
        m = START.match(raw)
        if m:
            t = tests.setdefault(int(m.group(1)), new_test())
            t["name"] = m.group(2)
            continue
        m = PREFIX.match(raw)
        if not m:
            continue
        t = tests.setdefault(int(m.group(1)), new_test())
        if has_sanitizer:
            t["sanitizer"] = True
        body = m.group(2)
        if DOCTEST_MARK.search(body):
            t["doctest_seen"] = True
        d = DOCTEST.search(body)
        if d:
            t["cases"] = int(d.group(1))
        da = DOCTEST_ASSERTS.search(body)
        if da and int(da.group(1)) > 0:
            t["failed_asserts"] += int(da.group(1))
        threw = DOCTEST_THREW.search(body)
        if threw:
            if REFUSAL.search(threw.group(1)):
                t["refused"] = True
            else:
                t["real_failure"] = True
        elif DOCTEST_REAL_FAIL.search(body):
            t["real_failure"] = True
        elif REFUSAL.search(body):
            t["refused"] = True
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
                if any(a.startswith("--test-case") for a in command):
                    t["expect_doctest"] = True
                if (t["expect_doctest"] and t.get("status") == "Passed" and t["cases"] is None):
                    problems.append("missing doctest execution summary: " + name)
        for name in wanted.keys() - seen:
            selected.append(new_test(name))
    incomplete = [t for t in selected if "status" not in t]
    non_passing = [t for t in selected if t.get("status") not in (None, "Passed", "Skipped")]
    refused = [t for t in non_passing if is_refused_failure(t)]
    failed = [t for t in non_passing if t not in refused]
    passed = [t for t in selected if t.get("status") in ("Passed", "Skipped")]
    for t in passed:
        if t["status"] == "Skipped" and not t["skips"]:
            t["skips"].append("other")
    empty = [t for t in passed if t["cases"] == 0]
    skipped = [t for t in passed if t["skips"] and t["cases"] != 0]
    executed = [t for t in passed if t not in empty and t not in skipped]
    code, rows = format_verdict(len(selected), executed, failed, refused, empty, skipped,
                                incomplete, problems, sanitizer, allow_skips, require_execution)
    return {"code": code, "rows": rows, "selected": selected, "executed": executed,
            "failed": failed, "refused": refused, "empty": empty, "skipped": skipped,
            "incomplete": incomplete, "problems": problems, "sanitizer": sanitizer}


def combine_retries(initial, retry_results, retry_rcs=None, allow_skips=("platform", "opt-in"),
                    require_execution=True):
    """Combines initial selection classification with per-test serial retry outcomes."""
    retry_rcs = retry_rcs or {}
    executed = list(initial["executed"])
    failed = list(initial["failed"])
    refused = []
    empty = list(initial["empty"])
    skipped = list(initial["skipped"])
    incomplete = list(initial["incomplete"])
    problems = list(initial["problems"])
    sanitizer = initial["sanitizer"]
    retried_passed = []
    refused_names = {t["name"] for t in initial["refused"]}
    for t in sorted(initial["refused"], key=lambda item: item["name"]):
        name = t["name"]
        if name not in retry_results:
            refused.append(t)
            continue
        r = retry_results[name]
        r_rc = retry_rcs.get(name, 0)
        sanitizer += r["sanitizer"]
        problems.extend(r["problems"])
        incomplete.extend(r["incomplete"])
        if r["problems"] or r["incomplete"]:
            continue
        if r["failed"]:
            failed.extend(r["failed"])
        elif r["refused"]:
            refused.extend(r["refused"])
        elif r["empty"]:
            empty.extend(r["empty"])
        elif r["skipped"]:
            skipped.extend(r["skipped"])
        elif r["executed"]:
            if r_rc:
                failed.append(dict(r["executed"][0], status="CTest returned " + str(r_rc)))
            else:
                executed.extend(r["executed"])
                if r["sanitizer"] == 0:
                    retried_passed.append(name)
        else:
            problems.append("empty retry section: " + name)
    for name in sorted(retry_results):
        if name not in refused_names:
            problems.append("unexpected retry section: " + name)
    code, rows = format_verdict(len(initial["selected"]), executed, failed, refused, empty, skipped,
                                incomplete, problems, sanitizer, allow_skips, require_execution,
                                retried_passed=retried_passed,
                                counts_label="COUNTS (after retry)")
    return {"code": code, "rows": rows, "selected": initial["selected"], "executed": executed,
            "failed": failed, "refused": refused, "empty": empty, "skipped": skipped,
            "incomplete": incomplete, "problems": problems, "sanitizer": sanitizer,
            "retried_passed": retried_passed}


def verdict(lines, expected=None, allow_skips=("platform", "opt-in"), require_execution=False):
    """Reads the lines of one log; returns (exit code, output lines)."""
    res = analyze(lines, expected, allow_skips, require_execution)
    return res["code"], res["rows"]


def extract_retry_sections(lines):
    """Extracts (selection_lines, [(name, section_lines), ...]) from a canonical gate log."""
    if "== selection" not in lines:
        return lines, []
    sel_start = lines.index("== selection") + 1
    markers = []
    for idx in range(sel_start, len(lines)):
        raw = lines[idx]
        if raw.startswith("== retry refused "):
            markers.append((idx, "retry", raw[len("== retry refused "):]))
        elif raw.startswith("== recording_after ") or raw.startswith("GATE_END "):
            markers.append((idx, "stop", ""))
            break
    if not markers:
        return lines[sel_start:], []
    sel_lines = lines[sel_start:markers[0][0]]
    retries = []
    for k, (pos, kind, name) in enumerate(markers):
        if kind != "retry":
            continue
        end = markers[k + 1][0] if k + 1 < len(markers) else len(lines)
        retries.append((name, lines[pos + 1:end]))
    return sel_lines, retries


def recorded_verdict(lines, expected=None, allow_skips=("platform", "opt-in")):
    """Classify tests without erasing a recorded gate failure or incomplete closeout."""
    ends = [(i, s[len("GATE_END "):]) for i, s in enumerate(lines) if s.startswith("GATE_END ")]
    canonical = any(s.startswith("== provenance ") for s in lines)
    require_exec = canonical or bool(ends)
    sel_lines, retry_blocks = extract_retry_sections(lines)
    if retry_blocks:
        initial = analyze(sel_lines, expected, allow_skips, require_exec)
        wanted = {t["name"]: t for t in expected} if expected is not None else None
        retry_results = {}
        duplicate_problems = []
        for name, r_lines in retry_blocks:
            if name in retry_results:
                duplicate_problems.append("duplicate retry section: " + name)
                continue
            if wanted is not None:
                r_expected = [wanted[name]] if name in wanted else []
            else:
                r_expected = None
            r_info = analyze(r_lines, r_expected, allow_skips, True)
            if wanted is None and (len(r_info["selected"]) != 1
                                   or r_info["selected"][0].get("name") != name):
                r_info["problems"].append("retry section mismatch for " + name)
            retry_results[name] = r_info
        combined = combine_retries(initial, retry_results, {}, allow_skips, require_exec)
        if duplicate_problems:
            combined["problems"].extend(duplicate_problems)
            code, out = format_verdict(
                len(combined["selected"]), combined["executed"], combined["failed"],
                combined["refused"], combined["empty"], combined["skipped"],
                combined["incomplete"], combined["problems"], combined["sanitizer"],
                allow_skips, require_exec, retried_passed=combined["retried_passed"],
                counts_label="COUNTS (after retry)")
        else:
            code, out = combined["code"], combined["rows"]
    else:
        code, out = verdict(lines, expected, allow_skips, require_execution=require_exec)
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
        expected = json.loads(args.inventory.read_text(encoding="utf-8"))["tests"] if args.inventory else None
        code, out = recorded_verdict(args.log.read_text(encoding="utf-8", errors="replace").splitlines(),
                                     expected, {"platform", "opt-in", *args.allow_skip})
    except (OSError, ValueError, KeyError, TypeError) as e:
        print("VERDICT: FAIL unreadable evidence: " + str(e))
        return 9
    print("\n".join(out))
    return code


if __name__ == "__main__":
    sys.exit(main())
