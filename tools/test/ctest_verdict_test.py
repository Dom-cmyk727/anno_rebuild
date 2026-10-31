#!/usr/bin/env python3
"""Contract tests of ctest_verdict.py: one synthetic ctest -V log per behaviour.

Every message below is a phrasing that exists in the suites run_selected.sh supports (the
audit is docs/perf/README.md, "MAP-2 repair 3"). Run: python3 -I tools/test/ctest_verdict_test.py
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ctest_verdict as cv  # noqa: E402

PRE = ["== preflight test_names_are_selectable", "1/1 Test #1: test_names_are_selectable ... Passed", "== selection"]


def log(*tests):
    """tests: (name, doctest case count or None, [message texts], status)."""
    lines = list(PRE)
    for n, (name, cases, messages, status) in enumerate(tests, start=10):
        lines.append("  %d/%d Test #%d: %s ........   %s    0.1 sec" % (n, len(tests), n, name,
                                                                         "Passed" if status == "Passed" else "***" + status))
        for text in messages:
            lines.append("%d: /src/t.cpp:1: MESSAGE: %s" % (n, text))
        if cases is not None:
            lines.append("%d: [doctest] test cases: %d | %d passed | 0 failed | 80 skipped" % (n, cases, cases))
    return lines


def one(messages, cases=1, status="Passed"):
    return cv.verdict(log(("t.case", cases, messages, status)))


class Contract(unittest.TestCase):
    def counts(self, out):
        return [l for l in out if l.startswith("COUNTS")][0]

    def test_real_execution_passes(self):
        code, out = one([])
        self.assertEqual(code, 0)
        self.assertIn("executed=1", self.counts(out))
        self.assertIn("skipped=0", self.counts(out))

    def test_doctest_filter_exclusions_are_not_skips(self):
        # "80 skipped" in every doctest line of the helper above
        code, out = one([])
        self.assertEqual(code, 0)
        self.assertIn("skipped=0", self.counts(out))

    def test_every_existing_missing_asset_phrasing_fails(self):
        forms = [
            "skipping: ANNO1503_GAME_DIR not set",
            "ANNO1503_GAME_DIR not set: skipped",  # bigbrother corpus
            "ANNO1503_GAME_DIR not set: skipping S01_07_01 search store",
            "skipping zei corpus: ANNO1503_GAME_DIR not set",
            "skipping the shipped BGruppen.dat: ANNO1503_GAME_DIR not set",
        ]
        for text in forms:
            code, out = one([text])
            self.assertEqual(code, 6, text)
            self.assertIn("required-asset=1", self.counts(out), text)
            self.assertIn("executed=0", self.counts(out), text)

    def test_platform_skips_are_reported_not_executed_and_do_not_fail(self):
        for text in ["skipping: WSL (Ubuntu) is not available",
                     "skipping: WSL (Ubuntu) is not available; the meter path is WSL",
                     "skipping on Windows: bInheritHandles = FALSE is at coord.cpp:142",
                     "skipped: the pipeline uses a POSIX shell"]:
            code, out = one([text])
            self.assertEqual(code, 0, text)
            self.assertIn("platform=1", self.counts(out), text)
            self.assertIn("executed=0", self.counts(out), text)

    def test_opt_in_skips_including_the_transport_statistics_message(self):
        for text in ["skipping: set ANNO_SIM_TRANSPORT_STATS=1 (and ANNO1503_GAME_DIR) to run",
                     "skipping: set ANNO_TEST_OLD_COORD=<an older anno_budget_coord> to run"]:
            code, out = one([text])
            self.assertEqual(code, 0, text)
            self.assertIn("opt-in=1", self.counts(out), text)
            self.assertIn("required-asset=0", self.counts(out), text)

    def test_gpu_skip_requires_a_declared_policy(self):
        code, out = one(["skipping: no GPU adapter"])
        self.assertEqual(code, 6)
        self.assertIn("gpu=1", self.counts(out))
        self.assertEqual(cv.verdict(log(("t.case", 1, ["skipping: no GPU adapter"], "Passed")),
                                    allow_skips=("gpu",))[0], 0)

    def test_item_outcomes_inside_an_executed_test_are_not_skips(self):
        for text in ["overlay-skip Tutorial/03T_Combat_01.sze buildings 13 any 0 origin 0",
                     "tile (99, 0) hold no PROD2 building item: skipped",
                     "ground Foo is on no read island; skipped as the original does"]:
            code, out = one([text])
            self.assertEqual(code, 0, text)
            self.assertIn("executed=1", self.counts(out), text)

    def test_a_zero_assertion_test_without_evidence_is_not_a_skip(self):
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

    def test_sanitizer_output_fails_even_when_the_test_passed(self):
        lines = log(("t.case", 1, [], "Passed"))
        lines.append("10: ==1234==ERROR: AddressSanitizer: heap-use-after-free")
        code, out = cv.verdict(lines)
        self.assertEqual(code, 4)
        lines = log(("t.case", 1, [], "Passed")) + ["10: x.cpp:3:5: runtime error: signed integer overflow"]
        self.assertEqual(cv.verdict(lines)[0], 4)

    def test_a_failed_test_fails(self):
        code, out = one([], status="Failed")
        self.assertEqual(code, 1)

    def test_precedence_a_missing_asset_beats_an_opt_in_in_the_same_test(self):
        code, out = cv.verdict(log(("t.case", 1, ["skipping: set ANNO_X=1 to run",
                                                  "skipping: ANNO1503_GAME_DIR not set"], "Passed")))
        self.assertEqual(code, 6)

    def test_inventory_requires_every_selected_terminal_result(self):
        expected = [{"name": "one"}, {"name": "two"}]
        code, out = cv.verdict(log(("one", 1, [], "Passed")), expected)
        self.assertEqual(code, 9)
        self.assertIn("selected=2", self.counts(out))
        self.assertIn("incomplete=1", self.counts(out))

    def test_started_test_without_terminal_result_fails_even_without_inventory(self):
        self.assertEqual(cv.verdict(PRE + ["  Start 1: unfinished"])[0], 9)

    def test_unexpected_and_duplicate_results_fail(self):
        lines = log(("one", 1, [], "Passed"))
        self.assertEqual(cv.verdict(lines, [{"name": "different"}])[0], 9)
        self.assertEqual(cv.verdict(lines + [lines[len(PRE)]])[0], 9)

    def test_expected_doctest_must_have_execution_summary(self):
        expected = [{"name": "one", "command": ["test-binary", "--test-case=one"]}]
        self.assertEqual(cv.verdict(log(("one", None, [], "Passed")), expected)[0], 9)

    def test_no_asset_ci_policy_keeps_skip_separate(self):
        lines = log(("corpus", 1, ["ANNO1503_GAME_DIR not set: skipped"], "Passed"))
        code, out = cv.verdict(lines, allow_skips=("required-asset",))
        self.assertEqual(code, 0)
        self.assertIn("executed=0", self.counts(out))
        self.assertIn("skipped=1", self.counts(out))

    def test_ctest_skip_status_is_never_an_executed_pass(self):
        code, out = cv.verdict(log(("one", None, [], "Skipped")))
        self.assertEqual(code, 6)
        self.assertIn("executed=0", self.counts(out))

    def test_plain_cmake_asset_skip_keeps_its_prerequisite(self):
        lines = PRE + ["  Start 1: corpus", "1: skipping: ANNO1503_GAME_DIR not set",
                       "1/1 Test #1: corpus ... ***Skipped 0.1 sec"]
        self.assertEqual(cv.verdict(lines)[0], 6)
        code, out = cv.verdict(lines, allow_skips=("required-asset",))
        self.assertEqual(code, 0)
        self.assertIn("required-asset=1", self.counts(out))
        self.assertIn("executed=0", self.counts(out))

    def test_re_read_preserves_failed_gate_despite_passing_children(self):
        lines = ["== provenance {}"] + log(("one", 1, [], "Passed"))
        self.assertEqual(cv.recorded_verdict(lines + ['GATE_END {"status":9,"ctest_rc":0}'])[0], 9)
        self.assertEqual(cv.recorded_verdict(lines + ['GATE_END {"status":1,"ctest_rc":8}'])[0], 1)
        self.assertEqual(cv.recorded_verdict(lines + ['GATE_END {"status":0,"ctest_rc":0}'])[0], 0)

    def test_missing_duplicate_contradictory_and_trailing_gate_records_fail(self):
        lines = ["== provenance {}"] + log(("one", 1, [], "Passed"))
        good = 'GATE_END {"status":0,"ctest_rc":0}'
        for tail in [[], [good, good], [good, "later data"], ['GATE_END invalid'],
                     ['GATE_END {"status":0,"ctest_rc":8}']]:
            self.assertEqual(cv.recorded_verdict(lines + tail)[0], 9)

    def test_canonical_success_cannot_claim_an_all_skipped_gate(self):
        body = log(("optional", 1, ["skipping: set ANNO_PROBE=1 to measure"], "Passed"))
        self.assertEqual(cv.recorded_verdict(body)[0], 0)  # Historical classification only.
        self.assertEqual(cv.recorded_verdict(["== provenance {}"] + body +
                         ['GATE_END {"status":0,"ctest_rc":0}'])[0], 8)


if __name__ == "__main__":
    unittest.main()
