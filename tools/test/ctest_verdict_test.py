#!/usr/bin/env python3
"""Contract tests of ctest_verdict.py: one synthetic ctest -V log per behaviour.

Ported from the MAP-2 contract (commit 8204b6b) with the rendering cases of round ec: a missing GPU
device, target hardware, the game-file phrasings of the render suites, and selected tests without a
terminal result. Run: python3 -I tools/test/ctest_verdict_test.py
"""
import os
import re
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ctest_verdict as cv  # noqa: E402

PRE = ["== preflight test_names_are_selectable", "1/1 Test #1: test_names_are_selectable ... Passed", "== selection"]


def log(*tests, finished=True):
    """tests: (name, doctest case count or None, [message texts], status)."""
    lines = list(PRE)
    for n, (name, cases, messages, status) in enumerate(tests, start=10):
        lines.append("        Start %d: %s" % (n, name))
        for text in messages:
            lines.append("%d: /src/t.cpp:1: MESSAGE: %s" % (n, text))
        if cases is not None:
            lines.append("%d: [doctest] test cases: %d | %d passed | 0 failed | 80 skipped" % (n, cases, cases))
        if finished:
            lines.append("  %d/%d Test #%d: %s ........   %s    0.1 sec" % (
                n, len(tests), n, name, "Passed" if status == "Passed" else "***" + status))
    if finished:
        bad = sum(1 for t in tests if t[3] not in ("Passed", "Skipped"))
        lines.append("%d%% tests passed, %d tests failed out of %d" % (
            100 * (len(tests) - bad) // max(1, len(tests)), bad, len(tests)))
    return lines


def one(messages, cases=1, status="Passed"):
    return cv.verdict(log(("t.case", cases, messages, status)))


class Contract(unittest.TestCase):
    def counts(self, out):
        return [l for l in out if l.startswith("COUNTS")][0]

    def test_real_execution_passes(self):
        code, out = one([])
        self.assertEqual(code, 0)
        self.assertIn("selected=1 executed=1", self.counts(out))
        self.assertIn("skipped=0", self.counts(out))

    def test_doctest_filter_exclusions_are_not_skips(self):
        code, out = one([])  # "80 skipped" in every doctest line of the helper above
        self.assertEqual(code, 0)
        self.assertIn("skipped=0", self.counts(out))

    def test_every_render_and_app_missing_asset_phrasing_fails(self):
        for text in ["skipping: ANNO1503_GAME_DIR not set", "skipping: no game files",
                     "skipping: Single_01.sze is not in this game directory",
                     "skipping: 06_1024_heavy_256.sze is not in this game directory"]:
            code, out = one([text])
            self.assertEqual(code, 6, text)
            self.assertIn("required-asset=1", self.counts(out), text)
            self.assertIn("executed=0", self.counts(out), text)

    def test_a_missing_gpu_device_fails_a_local_gate(self):
        code, out = one(["skipping: no GPU device: no adapter found"])
        self.assertEqual(code, 6)
        self.assertIn("required-capability=1", self.counts(out))
        self.assertIn("executed=0", self.counts(out))

    def test_target_hardware_is_a_platform_skip_not_a_pass(self):
        code, out = one(["skipping: no RTX 4090 among: Apple M5 Max (Metal)"])
        self.assertEqual(code, 0)
        self.assertIn("platform=1", self.counts(out))
        self.assertIn("executed=0", self.counts(out))
        self.assertIn("SKIPPED(platform): t.case", out)

    def test_opt_in_skips(self):
        code, out = one(["skipping: set ANNO_SIM_TRANSPORT_STATS=1 (and ANNO1503_GAME_DIR) to run"])
        self.assertEqual(code, 0)
        self.assertIn("opt-in=1", self.counts(out))
        self.assertIn("required-asset=0", self.counts(out))

    def test_other_explicit_skip_is_listed_and_does_not_fail(self):
        code, out = one(["skipping: the wave simulation did not start a background preparation here"])
        self.assertEqual(code, 0)
        self.assertIn("other=1", self.counts(out))
        self.assertIn("executed=0", self.counts(out))

    def test_item_outcomes_inside_an_executed_test_are_not_skips(self):
        for text in ["tile (99, 0) hold no PROD2 building item: skipped",
                     "ground Foo is on no read island; skipped as the original does"]:
            code, out = one([text])
            self.assertEqual(code, 0, text)
            self.assertIn("executed=1", self.counts(out), text)

    def test_a_zero_assertion_test_is_not_a_skip(self):
        code, out = one([], cases=1)
        self.assertEqual(code, 0)
        self.assertIn("skipped=0", self.counts(out))

    def test_zero_case_child_fails(self):
        code, out = one([], cases=0)
        self.assertEqual(code, 7)
        self.assertIn("empty=1", self.counts(out))

    def test_non_doctest_test_is_executed(self):
        code, out = one([], cases=None)
        self.assertEqual(code, 0)
        self.assertIn("executed=1", self.counts(out))

    def test_zero_selection_after_a_successful_preflight_fails(self):
        code, out = cv.verdict(PRE)
        self.assertEqual(code, 8)
        self.assertIn("selected=0", self.counts(out))

    def test_a_selected_test_without_a_terminal_result_fails(self):
        # The second test (number 11) started and printed, but has no result line (the run was cut).
        lines = log(("a.done", 1, [], "Passed")) + ["        Start 11: b.cut",
                                                   "11: [doctest] test cases: 1 | 1 passed | 0 failed | 80 skipped"]
        code, out = cv.verdict(lines)
        self.assertEqual(code, 9)
        self.assertIn("selected=2 executed=1", self.counts(out))
        self.assertIn("no_result=1", self.counts(out))
        self.assertIn("NO RESULT: b.cut", out)

    def test_sanitizer_output_fails_even_when_the_test_passed(self):
        lines = log(("t.case", 1, [], "Passed")) + ["10: ==1234==ERROR: AddressSanitizer: heap-use-after-free"]
        self.assertEqual(cv.verdict(lines)[0], 4)
        lines = log(("t.case", 1, [], "Passed")) + ["10: x.cpp:3:5: runtime error: signed integer overflow"]
        self.assertEqual(cv.verdict(lines)[0], 4)

    def test_failures_crashes_and_timeouts_fail(self):
        for status in ["Failed", "Timeout", "Exception", "SEGFAULT", "Not Run"]:
            code, out = one([], status=status)
            self.assertEqual(code, 1, status)
            self.assertIn("failed=1", self.counts(out), status)

    def test_precedence_a_missing_asset_beats_an_opt_in_in_the_same_test(self):
        code, out = cv.verdict(log(("t.case", 1, ["skipping: set ANNO_X=1 to run",
                                                  "skipping: ANNO1503_GAME_DIR not set"], "Passed")))
        self.assertEqual(code, 6)

    def test_the_round_eb_gpu_gate_log_is_read_per_test(self):
        # A saved real log when present: two known Metal failures, one platform skip.
        path = os.path.join(os.path.dirname(__file__), "..", "..", "cache", "eb-runs", "g3-dev-gpu.log")
        if not os.path.exists(path):
            self.skipTest("the round-eb log is not in this checkout")
        with open(path, errors="replace") as f:
            code, out = cv.verdict(f.read().split("\n"))
        self.assertEqual(code, 1)
        self.assertIn("selected=102 executed=99 failed=2 empty=0 no_result=0 skipped=1", self.counts(out))
        self.assertIn("platform=1", self.counts(out))


class ReviewOfA(unittest.TestCase):
    """Round ec review of package A: a cut log, CTest's own skip, the game-file phrasings."""

    def counts(self, out):
        return [l for l in out if l.startswith("COUNTS")][0]

    def test_a_log_cut_between_tests_is_not_a_pass(self):
        # One Start, one result of five, no summary (the reviewer's repro).
        lines = PRE + ["      Start 1: a.one", "1/5 Test #1: a.one .........   Passed    0.1 sec"]
        code, out = cv.verdict(lines)
        self.assertEqual(code, 9)
        self.assertIn("selected=5", self.counts(out))
        self.assertIn("4 test(s) CTest counted (of 5) have no line", "\n".join(out))

    def test_a_summary_counting_more_tests_than_the_log_shows_is_cut(self):
        lines = PRE + ["      Start 1: a.one", "1/1 Test #1: a.one .........   Passed    0.1 sec",
                       "100% tests passed, 0 tests failed out of 3"]
        code, out = cv.verdict(lines)
        self.assertEqual(code, 9)
        self.assertIn("selected=3", self.counts(out))

    def test_the_cmake_4_summary_without_a_failed_count_is_a_summary(self):
        # ctest 4.x writes "100% tests passed out of N" when nothing failed (the macOS runner).
        lines = log(("t.case", 1, [], "Passed"))[:-1] + ["100% tests passed out of 1"]
        code, out = cv.verdict(lines)
        self.assertEqual(code, 0)
        self.assertIn("selected=1 executed=1", self.counts(out))

    def test_a_cmake_4_summary_counting_more_tests_than_the_log_shows_is_cut(self):
        lines = PRE + ["      Start 1: a.one", "1/1 Test #1: a.one .........   Passed    0.1 sec",
                       "100% tests passed out of 3"]
        code, out = cv.verdict(lines)
        self.assertEqual(code, 9)
        self.assertIn("selected=3", self.counts(out))

    def test_a_selection_without_the_summary_line_is_cut(self):
        code, out = cv.verdict(log(("t.case", 1, [], "Passed"), finished=True)[:-1])
        self.assertEqual(code, 9)
        self.assertIn("no CTest summary line", "\n".join(out))

    def test_ctest_skip_of_a_missing_game_directory_is_a_required_prerequisite(self):
        lines = PRE + ["      Start 4: replay_case",
                       "4: skipping: ANNO1503_GAME_DIR not set",
                       "1/1 Test #4: replay_case ......................***Skipped   0.01 sec",
                       "100% tests passed, 0 tests failed out of 1"]
        code, out = cv.verdict(lines)
        self.assertEqual(code, 6)
        self.assertIn("required-asset=1", self.counts(out))
        self.assertIn("executed=0", self.counts(out))
        self.assertNotIn("NO RESULT", "\n".join(out))

    def test_ctest_skip_without_a_reason_is_listed_and_not_executed(self):
        lines = PRE + ["      Start 4: some_case",
                       "1/1 Test #4: some_case ......................***Skipped   0.01 sec",
                       "100% tests passed, 0 tests failed out of 1"]
        code, out = cv.verdict(lines)
        self.assertEqual(code, 0)
        self.assertIn("executed=0", self.counts(out))
        self.assertIn("other=1", self.counts(out))

    def test_the_scenario_phrasings_are_required_assets(self):
        for text in ["skipping: no scenario files", "skipping: no endless scenario"]:
            code, out = one([text])
            self.assertEqual(code, 6, text)
            self.assertIn("required-asset=1", self.counts(out), text)

    def test_every_skip_phrasing_in_the_sources_has_a_known_class(self):
        # A new phrasing that the reader would class "other" (a gate pass) must be listed here
        # with the reason it is not a prerequisite; the game-file and GPU ones fail the gate.
        allowed_other = {
            # A timing condition of one case, not an asset or a capability.
            "skipping: the wave simulation did not start a background preparation here",
            # A part of an executed case whose data has no road type.
            "skipping the road part: the registry has no road type",
            # The symlink rows of an executed case, where the platform refuses to create a
            # directory symlink (Windows without the privilege); the other rows still run.
            "skipping the symlink rows: ",
        }
        root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "src")
        literal = re.compile(r'"((?:skipping|skipped)[^"]*)"')
        found = {}
        for d, _, files in os.walk(root):
            for f in files:
                if f.endswith(".cpp"):
                    with open(os.path.join(d, f), errors="replace") as h:
                        for text in literal.findall(h.read()):
                            found[text] = os.path.join(d, f)
        self.assertGreater(len(found), 10)
        for text, where in sorted(found.items()):
            kind = cv.classify(text)
            self.assertIsNotNone(kind, "%s: %r is not read as a skip" % (where, text))
            if kind == "other":
                self.assertIn(text, allowed_other, "%s: %r is classed 'other'" % (where, text))


if __name__ == "__main__":
    unittest.main()
