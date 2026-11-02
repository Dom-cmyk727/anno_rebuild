#!/usr/bin/env python3
"""Public-command contract of tools/test/viewer_wiring.py (W4-WIN-FINAL F1): a child that exits 0 is not a
passing session unless the session record shows the invoked script ran. The command is run as a person runs
it, against a tiny fixture program that stands in for the window and writes (or withholds) the session
record; no game files and no window. Every broken-evidence case is preceded by the positive control in the
same class: the complete, matching record passes.

    python -I tools/test/viewer_wiring_contract_test.py"""
import os
import stat
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
WIRING = HERE / "tools" / "test" / "viewer_wiring.py"
TINY = HERE / "tools" / "test" / "viewer_wiring_fixtures" / "tiny.script"

# What the window records for tiny.script in the real format (see viewer_session.py): `ran line` for each
# non-wait step in order, a `checkpoint` line before its `ran line`.
GOOD = [
    "ran line 2: look 1",
    "ran line 3: key b",
    "checkpoint a: turn 1, frames 2, outcome none, visibility NoVisualChange, title: x",
    "ran line 5: checkpoint a",
    "ran line 6: quit",
]
MODES = {
    "complete": (GOOD, 0),
    "empty": ([], 0),
    "truncated": (GOOD[:-1], 0),
    "missing-middle": ([GOOD[0]] + GOOD[2:], 0),
    "reordered": ([GOOD[1], GOOD[0]] + GOOD[2:], 0),
    "wrong-script": (["ran line 2: look 9", "ran line 3: click 1", "ran line 4: quit"], 0),
    "extra-record": (GOOD + ["ran line 7: key n"], 0),
    "failed-step": (GOOD[:2] + ["FAIL line 3 (key): unknown key"] + GOOD[2:], 0),
    "child-fails": (GOOD, 7),
    "no-session-txt": (GOOD, 0),
    "noquit": (["ran line 1: look 1"], 0),
}

FIXTURE = r"""
import os, sys, time
from pathlib import Path
mode = os.environ["FIXTURE_MODE"]
session = Path(os.environ["ANNO_DEV_SESSION"])
if mode == "hang":
    time.sleep(60)
lines, code = eval(os.environ["FIXTURE_RECORD"])
if mode != "absent":
    session.mkdir(parents=True)
    (session / "script.log").write_text("\n".join(lines) + ("\n" if lines else ""), encoding="utf-8")
    if mode != "no-session-txt":
        (session / "session.txt").write_text("map lan5\nspeed 100\n", encoding="utf-8")
sys.exit(code)
"""


class ViewerWiringContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = Path(tempfile.mkdtemp(prefix="viewer_wiring_contract_"))
        (cls.tmp / "fixture.py").write_text(FIXTURE)
        if sys.platform == "win32":
            cls.exe = cls.tmp / "window.cmd"
            cls.exe.write_text(f'@echo off\r\n"{sys.executable}" "{cls.tmp / "fixture.py"}"\r\nexit /b %ERRORLEVEL%\r\n')
        else:
            cls.exe = cls.tmp / "window.sh"
            cls.exe.write_text(f'#!/bin/sh\nexec "{sys.executable}" "{cls.tmp / "fixture.py"}"\n')
            cls.exe.chmod(cls.exe.stat().st_mode | stat.S_IXUSR)
        cls.n = 0

    def run_case(self, mode, extra_env=None):
        type(self).n += 1
        out = self.tmp / f"session{self.n}"
        lines, code = MODES.get(mode, ([], 0))
        env = dict(os.environ, ANNO1503_GAME_DIR=str(self.tmp), FIXTURE_MODE=mode,
                   FIXTURE_RECORD=repr((lines, code)))
        env.update(extra_env or {})
        p = subprocess.run([sys.executable, "-I", str(WIRING), str(self.exe), str(TINY), str(out)],
                           env=env, capture_output=True, text=True, timeout=120)
        return p, out

    def test_control_a_complete_matching_record_passes(self):
        p, out = self.run_case("complete")
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertIn("PASS tiny", p.stdout)
        self.assertTrue((out / "script.log").is_file())

    def expect_failure(self, mode, fragment, extra_env=None):
        p, _ = self.run_case(mode, extra_env)
        self.assertEqual(p.returncode, 1, p.stdout + p.stderr)
        self.assertNotIn("PASS", p.stdout)
        self.assertIn(fragment, p.stdout, p.stdout)

    def test_a_child_that_never_records_a_session_does_not_pass(self):
        # The reproduced false pass: exit 0, no session directory at all.
        self.expect_failure("absent", "no script.log in the session directory")

    def test_an_empty_record_does_not_pass(self):
        self.expect_failure("empty", "script.log is empty")

    def test_a_truncated_record_does_not_pass(self):
        self.expect_failure("truncated", "truncated: 3 of the script's 4 non-wait steps")

    def test_a_record_with_a_step_missing_does_not_pass(self):
        self.expect_failure("missing-middle", "the script's step is")

    def test_a_reordered_record_does_not_pass(self):
        self.expect_failure("reordered", "out of order")

    def test_the_record_of_another_script_does_not_pass(self):
        self.expect_failure("wrong-script", "not of this script")

    def test_extra_records_do_not_pass(self):
        self.expect_failure("extra-record", "more records than the script has steps")

    def test_a_failed_step_does_not_pass_even_with_exit_0(self):
        self.expect_failure("failed-step", "a step failed: FAIL line 3 (key)")

    def test_a_failing_child_fails_with_its_status(self):
        self.expect_failure("child-fails", "window exit 7")

    def test_a_hung_window_is_killed_by_the_watchdog(self):
        started = time.monotonic()
        self.expect_failure("hang", "(watchdog)", {"ANNO_VIEWER_WIRING_WATCHDOG_S": "2"})
        self.assertLess(time.monotonic() - started, 30.0)  # killed promptly, not when the fixture's 60 s sleep ends

    def test_a_record_without_the_window_session_files_does_not_pass(self):
        self.expect_failure("no-session-txt", "no session.txt in the session directory")

    def test_a_script_without_a_terminal_quit_is_not_a_complete_run(self):
        noquit = self.tmp / "noquit.script"
        noquit.write_text("look 1 2\n")
        type(self).n += 1
        out = self.tmp / f"session{self.n}"
        env = dict(os.environ, ANNO1503_GAME_DIR=str(self.tmp), FIXTURE_MODE="noquit",
                   FIXTURE_RECORD=repr(MODES["noquit"]))
        p = subprocess.run([sys.executable, "-I", str(WIRING), str(self.exe), str(noquit), str(out)],
                           env=env, capture_output=True, text=True, timeout=120)
        self.assertEqual(p.returncode, 1, p.stdout + p.stderr)
        self.assertIn("does not end with quit", p.stdout)

    def test_a_missing_game_dir_is_a_named_skip_not_a_pass(self):
        env = {k: v for k, v in os.environ.items() if k != "ANNO1503_GAME_DIR"}
        p = subprocess.run([sys.executable, "-I", str(WIRING), str(self.exe), str(TINY), str(self.tmp / "nogame")],
                           env=env, capture_output=True, text=True, timeout=60)
        self.assertEqual(p.returncode, 77, p.stdout + p.stderr)
        self.assertIn("skipping: ANNO1503_GAME_DIR is not set", p.stdout)


if __name__ == "__main__":
    unittest.main()
