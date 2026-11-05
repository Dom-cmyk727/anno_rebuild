#!/usr/bin/env python3
"""Public-command contract of tools/forester_ref.py (stdlib only, no framework).

  forester_ref_contract_test.py --ref <path/to/forester_ref.py> --vectors <path/to/forester_vectors.txt>
      [--min-vectors 45] [--timeout 20]
  (or the environment variables FORESTER_REF and FORESTER_VECTORS; the CTest registration passes absolute paths.)

Every check runs the reference as a bounded child process of THIS script (its own interpreter, a hard timeout);
on a timeout only that exact child (its pid, from Popen) is killed and the check fails with a clear message, so a
case can never pass by timing out. Exit status of this script: 0 all checks passed, 1 a check failed.

Checks (the review of 2026-11-03, C1-C3):
  empty input                      must FAIL (exit 2, message "0 vectors executed")
  type-directive-only input        must FAIL (exit 2, same message)
  missing file                     must FAIL (exit 2)
  full-output drain 4096           must COMPLETE (exit 0, within the timeout) and print the matched amounts 1152 1288
  correct case                     exit 0, "1 vectors, 0 failures"
  incorrect expectation            exit 1 with a FAIL line naming the computed value ("got=0 7 0 1")
  malformed vector line            exit 1 (an error is a failed vector, never a skip)
  bundled vector file              exit 0; executed count == the number of vector lines in the file (>= --min-vectors),
                                   one PASS line per vector, no FAIL line
  E2.0 mutants (a patched COPY of the reference, run as a child; the patch target must exist exactly once):
    wrong placement clear          `claims faithful A+ P -> 1 -` FAILS (got 0) while `claims faithful A+ A- -> 0 -` PASSES
    first delay burned per visit   `visits ... | 1 1 1 1 1` FAILS while the one 5-tick visit PASSES
    first delay not saved          `visits ... | 2 S 3` FAILS while the unsaved `| 2 3` PASSES
    first delay re-armed at Start  `visits 768 ... | 50 9000` FAILS (960) while `| 50` PASSES
"""
import argparse
import os
import subprocess
import sys
import tempfile
import unittest

REF = None
VECTORS = None
TIMEOUT = 20.0
MIN_VECTORS = 75

# E2.0 mutation targets in tools/forester_ref.py and their wrong replacements (each target must occur exactly once).
PLACEMENT_KEEPS = ('        if op in ("P", "C"):\n'
                   '            if mode == "enhanced":\n')
PLACEMENT_CLEARS = ('        if op in ("P", "C"):\n'
                    '            if True:\n')
DELAY_OWED = ('                    if st["delay"] > 0:\n'
              '                        return None\n')
DELAY_BURNED = ('                    if st["delay"] > 0:\n'
                '                        st["delay"] = START_DELAY\n'
                '                        return None\n')
SAVED_WITH_DELAY = 'SAVE_FIELDS = ("delay", '
SAVED_WITHOUT_DELAY = 'SAVE_FIELDS = ('
NEXT_STEP = '                    st["step"] = "Start" if cap > st["cargo"] else "ToHome"\n'
NEXT_STEP_REARMED = (NEXT_STEP +
                     '                    st["delay"] = START_DELAY if st["step"] == "Start" else 0\n')


def run_ref(path, ref=None):
    """Run the reference (or the patched copy `ref`) on `path` as an owned, bounded child. Returns (returncode,
    stdout, stderr). Raises AssertionError on a timeout after killing exactly that child."""
    p = subprocess.Popen([sys.executable, ref or REF, path], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                         text=True)
    try:
        out, err = p.communicate(timeout=TIMEOUT)
    except subprocess.TimeoutExpired:
        pid = p.pid
        p.kill()                       # exactly the child we started; never a name-pattern kill
        try:
            p.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            pass
        raise AssertionError("forester_ref.py did not finish within %.0f s on %s (child pid %d killed)"
                             % (TIMEOUT, path, pid))
    return p.returncode, out, err


def vector_lines(path):
    n = 0
    with open(path) as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#") and not line.startswith("type "):
                n += 1
    return n


class ForesterRefContract(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)

    def write(self, name, text):
        p = os.path.join(self.tmp.name, name)
        with open(p, "w") as f:
            f.write(text)
        return p

    def test_empty_file_fails(self):
        rc, out, _ = run_ref(self.write("empty.txt", ""))
        self.assertEqual(rc, 2, out)
        self.assertIn("0 vectors executed", out)

    def test_comments_only_fails(self):
        rc, out, _ = run_ref(self.write("c.txt", "# nothing but a comment\n\n"))
        self.assertEqual(rc, 2, out)
        self.assertIn("0 vectors executed", out)

    def test_type_directive_only_fails(self):
        rc, out, _ = run_ref(self.write("t.txt", "type 104 ROHSTWACHS\ntype 105 ROHSTOFF BAUM\n"))
        self.assertEqual(rc, 2, out)
        self.assertIn("0 vectors executed", out)

    def test_missing_file_fails(self):
        rc, out, _ = run_ref(os.path.join(self.tmp.name, "does-not-exist.txt"))
        self.assertEqual(rc, 2, out)
        self.assertIn("cannot read", out)

    def test_full_output_drain_completes_with_matched_amounts(self):
        # hand-derived: 92 steps make 14 * 92 = 1288 (cap 1280 reached), raw 4096 - 92 * 32 = 1152
        rc, out, _ = run_ref(self.write("d.txt", "drain 4096 -> 1152 1288\n"))
        self.assertEqual(rc, 0, out)
        self.assertIn("PASS drain 4096 -> 1152 1288", out)
        self.assertIn("1 vectors, 0 failures", out)

    def test_full_output_drain_wrong_amounts_fail(self):
        rc, out, _ = run_ref(self.write("d2.txt", "drain 4096 -> 1152 1289\n"))
        self.assertEqual(rc, 1, out)
        self.assertIn("got=1152 1288", out)

    def test_correct_case_passes(self):
        rc, out, _ = run_ref(self.write("ok.txt", "step 16 0 0 -> 0 7 0 1\n"))
        self.assertEqual(rc, 0, out)
        self.assertIn("1 vectors, 0 failures", out)

    def test_incorrect_expectation_fails_with_computed_value(self):
        rc, out, _ = run_ref(self.write("bad.txt", "step 16 0 0 -> 0 8 0 1\n"))
        self.assertEqual(rc, 1, out)
        self.assertIn("FAIL step 16 0 0 -> 0 8 0 1", out)
        self.assertIn("got=0 7 0 1", out)
        self.assertIn("1 vectors, 1 failures", out)

    def test_malformed_line_is_a_failure_not_a_skip(self):
        rc, out, _ = run_ref(self.write("m.txt", "step 16 0 -> 0 7 0 1\nstep 16 0 0 -> 0 7 0 1\n"))
        self.assertEqual(rc, 1, out)
        self.assertIn("2 vectors, 1 failures", out)

    def test_unknown_command_is_a_failure(self):
        rc, out, _ = run_ref(self.write("u.txt", "frobnicate 1 -> 2\n"))
        self.assertEqual(rc, 1, out)
        self.assertIn("UNKNOWN command", out)

    def test_every_command_rejects_a_wrong_expectation(self):
        # A reference that echoes the expected answer (got = exp) would pass every vector. Corrupt the expected side
        # of EVERY bundled vector (append a token) and require every one to fail, so each command has a negative control.
        lines = []
        types = []
        with open(VECTORS) as f:
            for line in f:
                line = line.strip()
                if line.startswith("type "):
                    types.append(line)
                elif line and not line.startswith("#"):
                    lines.append(line + " 999")
        rc, out, _ = run_ref(self.write("mut.txt", "\n".join(types + lines) + "\n"))
        self.assertEqual(rc, 1, out[-1500:])
        self.assertIn("%d vectors, %d failures" % (len(lines), len(lines)), out)
        self.assertEqual(sum(1 for l in out.splitlines() if l.startswith("PASS ")), 0)
        cmds = {l.split()[0] for l in lines}
        for c in ("conv", "step", "area", "harvest", "cand", "candskip", "grow", "drain", "visit", "visitstale",
                  "claims", "draws", "walkcode", "goalfree", "visits"):
            self.assertIn(c, cmds, "bundled vectors lack command " + c)

    def test_visit_overlay_and_stale_differ(self):
        rc, out, _ = run_ref(self.write("v.txt", "visit 768 6500 40000 | 3 7 -> 512 Done 20000 - 3,7\n"
                                                  "visitstale 768 6500 40000 | 3 7 -> 768 Done 17500 - 3,3,3\n"
                                                  "visitstale 768 6500 40000 | 3 7 -> 512 Done 20000 - 3,7\n"))
        self.assertEqual(rc, 1, out)
        self.assertIn("3 vectors, 1 failures", out)
        self.assertIn("got=768 Done 17500 - 3,3,3", out)

    def mutant(self, old, new, vectors):
        """Runs a copy of the reference with `old` replaced by `new` on `vectors` (a child process, as run_ref)."""
        with open(REF) as f:
            src = f.read()
        self.assertEqual(src.count(old), 1, "mutation target not found exactly once: %r" % old)
        ref = self.write("mutant_ref.py", src.replace(old, new))
        return run_ref(self.write("mutant_vectors.txt", vectors), ref=ref)

    def test_mutant_placement_clears_claim_is_detected(self):
        # The withdrawn reading (placement clears bit 29) must fail the short discriminating vector while the
        # legal worker unclaim stays green (docs/sim/forestry.md E-16; review 2026-11-03, first P2).
        rc, out, _ = self.mutant(PLACEMENT_KEEPS, PLACEMENT_CLEARS,
                                 "claims faithful A+ P -> 1 -\nclaims faithful A+ A- -> 0 -\n")
        self.assertEqual(rc, 1, out)
        self.assertIn("FAIL claims faithful A+ P -> 1 -", out)
        self.assertIn("got=0 -", out)
        self.assertIn("PASS claims faithful A+ A- -> 0 -", out)
        self.assertIn("2 vectors, 1 failures", out)

    def test_mutant_first_delay_burned_per_visit_is_detected(self):
        # The rejected rule: a visit below the delay burns its ticks and the next one owes 5 again.
        rc, out, _ = self.mutant(DELAY_OWED, DELAY_BURNED,
                                 "visits 256 6500 5 | 3 | 1 1 1 1 1 -> 0 0 ToTree 1500 3 -\n"
                                 "visits 256 6500 5 | 3 | 5 -> 0 0 ToTree 1500 3 -\n")
        self.assertEqual(rc, 1, out)
        self.assertIn("FAIL visits 256 6500 5 | 3 | 1 1 1 1 1", out)
        self.assertIn("got=5 0 Start 0 - -", out)
        self.assertIn("PASS visits 256 6500 5 | 3 | 5 ->", out)
        self.assertIn("2 vectors, 1 failures", out)

    def test_mutant_first_delay_not_saved_is_detected(self):
        rc, out, _ = self.mutant(SAVED_WITH_DELAY, SAVED_WITHOUT_DELAY,
                                 "visits 256 6500 5 | 3 | 2 S 3 -> 0 0 ToTree 1500 3 -\n"
                                 "visits 256 6500 5 | 3 | 2 3 -> 0 0 ToTree 1500 3 -\n")
        self.assertEqual(rc, 1, out)
        self.assertIn("FAIL visits 256 6500 5 | 3 | 2 S 3", out)
        self.assertIn("got=2 0 Start 0 - -", out)
        self.assertIn("PASS visits 256 6500 5 | 3 | 2 3 ->", out)
        self.assertIn("2 vectors, 1 failures", out)

    def test_mutant_first_delay_rearmed_after_cut_is_detected(self):
        rc, out, _ = self.mutant(NEXT_STEP, NEXT_STEP_REARMED,
                                 "visits 768 6500 5 | 3 7 | 50 9000 -> 0 256 ToTree 955 7 3\n"
                                 "visits 256 6500 5 | 3 | 50 -> 0 0 ToTree 1455 3 -\n")
        self.assertEqual(rc, 1, out)
        self.assertIn("FAIL visits 768 6500 5 | 3 7 | 50 9000", out)
        self.assertIn("got=0 256 ToTree 960 7 3", out)
        self.assertIn("PASS visits 256 6500 5 | 3 | 50 ->", out)
        self.assertIn("2 vectors, 1 failures", out)

    def test_bundled_vectors_all_executed(self):
        expected = vector_lines(VECTORS)
        self.assertGreaterEqual(expected, MIN_VECTORS,
                                "the bundled file has %d vector lines, fewer than %d" % (expected, MIN_VECTORS))
        rc, out, _ = run_ref(VECTORS)
        self.assertEqual(rc, 0, out[-2000:])
        self.assertIn("%d vectors, 0 failures" % expected, out)
        lines = out.splitlines()
        self.assertEqual(sum(1 for l in lines if l.startswith("PASS ")), expected)
        self.assertEqual(sum(1 for l in lines if l.startswith("FAIL ")), 0)
        # the new full-output vector is among them
        self.assertIn("PASS drain 4096 -> 1152 1288", out)


def main():
    global REF, VECTORS, TIMEOUT, MIN_VECTORS
    ap = argparse.ArgumentParser()
    ap.add_argument("--ref", default=os.environ.get("FORESTER_REF"))
    ap.add_argument("--vectors", default=os.environ.get("FORESTER_VECTORS"))
    ap.add_argument("--min-vectors", type=int, default=MIN_VECTORS)
    ap.add_argument("--timeout", type=float, default=TIMEOUT)
    args, rest = ap.parse_known_args()
    if not args.ref or not args.vectors:
        print("error: pass --ref and --vectors (or FORESTER_REF / FORESTER_VECTORS)", file=sys.stderr)
        return 2
    REF, VECTORS = os.path.abspath(args.ref), os.path.abspath(args.vectors)
    TIMEOUT, MIN_VECTORS = args.timeout, args.min_vectors
    for p in (REF, VECTORS):
        if not os.path.isfile(p):
            print("error: not a file: " + p, file=sys.stderr)
            return 2
    res = unittest.main(argv=[sys.argv[0]] + rest, exit=False, verbosity=2).result
    if res.testsRun == 0:
        print("error: no checks ran", file=sys.stderr)
        return 1
    return 0 if res.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
