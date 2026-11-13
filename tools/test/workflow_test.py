#!/usr/bin/env python3
"""Public gate contracts using a tiny CTest project: no compiler, GPU or game files."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
GATE = ROOT / "tools/test/run_selected.py"


class Workflow(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="anno-gate-contract-")
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.source = Path(cls.tmp.name)
        cls.build = cls.source / "build"
        (cls.source / "src").mkdir()
        cls.env = dict(os.environ)
        cls.env.pop("ANNO1503_GAME_DIR", None)
        (cls.source / ".gitignore").write_text("build/\nlogs/\n*.run.lock/\n")
        (cls.source / "case.py").write_text('''import pathlib, sys
mode, marker = sys.argv[1:3]
# Appended, not overwritten: a marker with more than one line means this case ran more than
# once in a single gate invocation (setUp() removes it before each test).
with pathlib.Path(marker).open("a") as f:
    f.write(mode + "\\n")
if mode == "empty":
    print("[doctest] test cases: 0 | 0 passed | 0 failed | 12 skipped")
elif mode not in ("missing_summary", "cmake_skip"):
    print("[doctest] test cases: 1 | 1 passed | 0 failed | 12 skipped")
if mode == "asset": print("/t.cpp:1: MESSAGE: ANNO1503_GAME_DIR not set: skipped")
if mode == "gpu": print("/t.cpp:1: MESSAGE: skipping: no GPU adapter")
if mode == "asan": print("ERROR: AddressSanitizer: heap-use-after-free")
if mode == "cmake_skip": print("skipping: ANNO1503_GAME_DIR not set")
if mode == "mutate": (pathlib.Path(marker).parent.parent / "changed.txt").write_text("changed")
sys.exit(1 if mode == "failure" else 0)
''')
        cmake = 'cmake_minimum_required(VERSION 3.25)\nproject(Gate NONE)\nenable_testing()\n'
        for mode in ["pass", "empty", "asset", "gpu", "asan", "failure", "missing_summary", "cmake_skip", "mutate"]:
            cmake += ('add_test(NAME ' + mode + ' COMMAND "' + Path(sys.executable).as_posix()
                      + '" "${CMAKE_SOURCE_DIR}/case.py" ' + mode
                      + ' "${CMAKE_BINARY_DIR}/' + mode + '.ran" --test-case=' + mode + ')\n')
        cmake += 'set_tests_properties(cmake_skip PROPERTIES SKIP_REGULAR_EXPRESSION "skipping:")\n'
        (cls.source / "CMakeLists.txt").write_text(cmake)
        for command in [
            ["git", "init", "-q"], ["git", "add", "."],
            ["git", "-c", "user.name=Gate test", "-c", "user.email=gate@example.invalid",
             "commit", "-qm", "synthetic gate fixture"],
            ["cmake", "-S", str(cls.source), "-B", str(cls.build)],
        ]:
            p = subprocess.run(command, cwd=cls.source, env=cls.env, capture_output=True, text=True)
            if p.returncode:
                raise RuntimeError(p.stdout + p.stderr)

    def setUp(self):
        self.log = self.source / "logs" / (self.id().split(".")[-1] + ".log")
        for p in self.build.glob("*.ran"):
            p.unlink()

    def invoke(self, select="^pass$", *extra):
        command = [sys.executable, "-I", str(GATE), "--source-root", str(self.source),
                   "--build-dir", str(self.build), "--select", select, "--log", str(self.log),
                   "--timeout", "15", "--config", "Release", *extra]
        return subprocess.run(command, cwd=self.source, env=self.env, capture_output=True,
                              text=True, timeout=45)

    def test_pass_records_inventory_and_provenance(self):
        p = self.invoke()
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        inv = json.loads(Path(str(self.log) + ".inventory.json").read_text())
        self.assertEqual([t["name"] for t in inv["tests"]], ["pass"])
        self.assertIn('"binaries":', self.log.read_text())
        self.assertIn("selected=1 executed=1", p.stdout)

    def test_empty_child_fails(self):
        self.assertEqual(self.invoke("^empty$").returncode, 7)

    def test_sanitizer_with_success_exit_fails(self):
        self.assertEqual(self.invoke("^asan$").returncode, 4)

    def test_failed_child_fails(self):
        self.assertEqual(self.invoke("^failure$").returncode, 1)

    def test_missing_doctest_summary_fails(self):
        self.assertEqual(self.invoke("^missing_summary$").returncode, 9)

    def test_required_assets_fail(self):
        self.assertEqual(self.invoke("^asset$").returncode, 6)

    def test_ci_assets_policy_keeps_executed_and_skipped_apart(self):
        p = self.invoke("^(pass|asset)$", "--asset-policy", "unavailable")
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertIn("selected=2 executed=1", p.stdout)
        self.assertIn("required-asset=1", p.stdout)

    def test_gpu_prerequisite_requires_explicit_policy(self):
        self.assertEqual(self.invoke("^gpu$").returncode, 6)

    def test_even_allowed_skips_alone_do_not_establish_execution(self):
        self.assertEqual(self.invoke("^gpu$", "--allow-skip", "gpu").returncode, 8)

    def test_plain_cmake_skip_obeys_ci_policy(self):
        p = self.invoke("^(pass|cmake_skip)$", "--asset-policy", "unavailable")
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertIn("selected=2 executed=1", p.stdout)
        self.assertIn("required-asset=1", p.stdout)

    def test_re_read_cannot_erase_gate_failure_after_passing_tests(self):
        try:
            p = self.invoke("^mutate$")
            self.assertEqual(p.returncode, 9, p.stdout + p.stderr)
            again = subprocess.run([sys.executable, "-I", str(ROOT / "tools/test/ctest_verdict.py"),
                                    str(self.log), "--inventory", str(self.log) + ".inventory.json"],
                                   capture_output=True, text=True)
            self.assertEqual(again.returncode, 9, again.stdout + again.stderr)
            self.assertIn("recorded gate status=9", again.stdout)
        finally:
            (self.source / "changed.txt").unlink(missing_ok=True)

    def test_no_selection_fails(self):
        self.assertNotEqual(self.invoke("^absent$").returncode, 0)
        self.assertNotIn("== selection\n", self.log.read_text())

    def test_missing_required_contract_blocks_selection(self):
        p = self.invoke("^pass$", "--require-test", "absent")
        self.assertNotEqual(p.returncode, 0)
        self.assertFalse((self.build / "pass.ran").exists())

    def test_failing_required_contract_blocks_selection(self):
        p = self.invoke("^pass$", "--require-test", "failure")
        self.assertNotEqual(p.returncode, 0)
        self.assertFalse((self.build / "pass.ran").exists())
        self.assertTrue((self.build / "failure.ran").exists())

    def test_required_contract_does_not_inflate_selection(self):
        p = self.invoke("^pass$", "--require-test", "pass")
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertIn("selected=0 executed=0", p.stdout)

    def test_required_and_selected_contract_runs_once(self):
        # GATE-1b (project analysis §2.1/§2.2, finding D2): a test that is both --require-test'd
        # and matched by --select used to run twice (once as the required contract, once again in
        # the selection run); it must now run exactly once.
        p = self.invoke("^pass$", "--require-test", "pass")
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        marker = (self.build / "pass.ran").read_text()
        self.assertEqual(marker.count("\n"), 1, marker)
        self.assertIn("== required contract pass\n", self.log.read_text())
        self.assertIn("(every selected test already ran as a required contract above)", self.log.read_text())

    def test_empty_selection_with_require_test_still_fails(self):
        # GATE-1c item 3: GATE-1b's required-contract exclusion must not resurrect a --select
        # that never matched anything (distinct from one that matched only required contracts,
        # which test_required_contract_does_not_inflate_selection above keeps as a PASS).
        p = self.invoke("^absent$", "--require-test", "pass")
        self.assertNotEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertNotIn("== selection\n", self.log.read_text())

    def test_skip_auto_contract_leaves_it_out(self):
        cmake = self.source / "src/tools/bench_picture/CMakeLists.txt"
        cmake.parent.mkdir(parents=True)
        cmake.write_text("# renderer branch fixture\n")
        try:
            if os.name != "nt":
                p = self.invoke("^pass$", "--skip-auto-contract")
                self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
                self.assertNotIn("picture_matrix_contract", self.log.read_text())
        finally:
            shutil.rmtree(self.source / "src/tools")

    def test_name_preflight_catches_multiline_semicolon_before_build(self):
        bad = self.source / "src/bad.cpp"
        bad.write_text('TEST_CASE(\n "broken"\n ";name") {}\n')
        try:
            p = self.invoke("^pass$", "--target", "missing-target")
            self.assertEqual(p.returncode, 5)
            self.assertIn("mandatory source/verdict preflight failed", p.stderr)
            self.assertNotIn('"--build"', self.log.read_text())
            self.assertFalse((self.build / "pass.ran").exists())
        finally:
            bad.unlink()

    def test_build_failure_blocks_selection(self):
        self.assertEqual(self.invoke("^pass$", "--target", "missing-target").returncode, 2)
        self.assertFalse((self.build / "pass.ran").exists())

    def test_existing_log_is_not_overwritten(self):
        self.log.parent.mkdir(exist_ok=True)
        self.log.write_text("earlier failure\n")
        self.assertNotEqual(self.invoke().returncode, 0)
        self.assertEqual(self.log.read_text(), "earlier failure\n")

    def test_own_untracked_output_is_not_a_source_mutation(self):
        self.log = self.source / "owned-evidence.txt"
        try:
            p = self.invoke()
            self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        finally:
            self.log.unlink(missing_ok=True)
            Path(str(self.log) + ".inventory.json").unlink(missing_ok=True)

    def test_existing_build_lock_is_not_removed(self):
        lock = Path(str(self.build) + ".run.lock")
        lock.mkdir()
        try:
            self.assertNotEqual(self.invoke().returncode, 0)
            self.assertTrue(lock.exists())
        finally:
            lock.rmdir()

    def test_renderer_contract_required_even_if_unregistered(self):
        cmake = self.source / "src/tools/bench_picture/CMakeLists.txt"
        cmake.parent.mkdir(parents=True)
        cmake.write_text("# renderer branch fixture\n")
        try:
            if os.name != "nt":
                p = self.invoke()
                self.assertNotEqual(p.returncode, 0)
                self.assertIn("missing required contract: picture_matrix_contract", p.stderr)
                self.assertFalse((self.build / "pass.ran").exists())
        finally:
            shutil.rmtree(self.source / "src/tools")


if __name__ == "__main__":
    unittest.main()
