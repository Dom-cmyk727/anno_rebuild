#!/usr/bin/env python3
"""Public-command contracts of the native Windows window tools (W4-WIN-CLOSE C4): watchdog termination,
an unreadable peak memory measurement and a refused capture, each with a successful control, through
tools/bench/window_case.py and tools/bench/dev_session_capture.py as a person runs them, against tiny
fixture programs (no game, no GPU, seconds). Windows only: the tools use Win32 and the capture needs a
desktop; elsewhere the contract is reported as not run (the UNIX window tools have their own shell
contracts). Run: python -I tools/test/window_tools_contract_test.py (Pillow for the capture cases)."""
import importlib.util
import os
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
CASE = HERE / "tools" / "bench" / "window_case.py"
CAPTURE = HERE / "tools" / "bench" / "dev_session_capture.py"
TABLE = HERE / "tools" / "bench" / "window_table.py"


def load_table():
    spec = importlib.util.spec_from_file_location("window_table_under_test", TABLE)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


@unittest.skipUnless(sys.platform == "win32", "skipping: the native Windows window tools need Windows")
class WindowCase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = Path(tempfile.mkdtemp(prefix="window_tools_contract_"))
        # Fixture "windows": a command script that ends at once with a given status, and one that
        # outlives any watchdog the tests give it (ping as a sleep; it ends on its own after 25 s).
        cls.ok = cls.tmp / "ok.cmd"
        cls.ok.write_text("@echo off\r\nexit /b 0\r\n")
        cls.five = cls.tmp / "five.cmd"
        cls.five.write_text("@echo off\r\nexit /b 5\r\n")
        cls.slow = cls.tmp / "slow.cmd"
        cls.slow.write_text("@echo off\r\nping -n 25 127.0.0.1 >nul\r\nexit /b 0\r\n")
        cls.commands = cls.tmp / "commands.txt"
        cls.commands.write_text("# none\n")
        cls.n = 0

    def run_case(self, app, seconds="1", extra_env=None):
        type(self).n += 1
        out = self.tmp / f"case{self.n}"
        env = dict(os.environ, ANNO1503_GAME_DIR=str(self.tmp))
        env.update(extra_env or {})
        started = time.monotonic()
        p = subprocess.run([sys.executable, "-I", str(CASE), str(app), sys.executable, str(out),
                            str(self.commands), "800x600", seconds, "1,1", "off"],
                           env=env, capture_output=True, text=True, timeout=120)
        return p, out, time.monotonic() - started

    @staticmethod
    def run_txt(out):
        return (out / "run.txt").read_text()

    def test_control_a_window_that_ends_well_is_a_measured_success(self):
        p, out, _ = self.run_case(self.ok)
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertIn("window_exit=0 child_exit=0", self.run_txt(out))
        peak = (out / "time.txt").read_text()
        self.assertRegex(peak, r"Peak working set size \(bytes\): [1-9]\d*")
        # The table accepts the measurement.
        self.assertRegex(load_table().peak_rss(out), r"^\d+\.\d\d GiB$")

    def test_the_windows_own_status_is_kept(self):
        p, out, _ = self.run_case(self.five)
        self.assertEqual(p.returncode, 5, p.stderr)
        self.assertIn("window_exit=5 child_exit=5", self.run_txt(out))
        self.assertTrue((out / "time.txt").is_file())

    def test_the_watchdog_kills_a_window_that_does_not_end(self):
        p, out, elapsed = self.run_case(self.slow, seconds="1", extra_env={"ANNO_WINDOW_CASE_GRACE_S": "1"})
        self.assertEqual(p.returncode, 124, p.stderr)
        self.assertIn("window_exit=124 child_exit=killed", self.run_txt(out))
        self.assertIn("was killed (status 124)", p.stderr)  # a useful diagnostic
        self.assertLess(elapsed, 20.0)  # 2 s limit, not the fixture's 25 s
        # (What a matrix does with window_exit=124 is window_table's rule, covered by its own
        # contract, window_table_test.sh: a failed run is exit 6, never a success row.)

    def test_an_unreadable_peak_is_not_a_measured_run(self):
        p, out, _ = self.run_case(self.ok, extra_env={"ANNO_WINDOW_CASE_NO_PEAK": "1"})
        self.assertEqual(p.returncode, 3, p.stderr)  # not the window's 0
        self.assertIn("window_exit=3 child_exit=0", self.run_txt(out))  # the window's own 0 is kept apart
        self.assertIn("peak working set could not be read", p.stderr)
        # And what it left behind is refused by the table, never read as 0 GiB.
        with self.assertRaises(Exception) as caught:
            load_table().peak_rss(out)
        self.assertIn("missing", str(caught.exception))

    def test_an_unreadable_peak_does_not_hide_the_windows_own_failure(self):
        p, out, _ = self.run_case(self.five, extra_env={"ANNO_WINDOW_CASE_NO_PEAK": "1"})
        self.assertEqual(p.returncode, 5, p.stderr)  # the window's failure stands, not 3
        self.assertIn("window_exit=5 child_exit=5", self.run_txt(out))

    def test_a_zero_peak_in_time_txt_is_unusable(self):
        case = self.tmp / "zero"
        case.mkdir()
        (case / "time.txt").write_text("Peak working set size (bytes): 0\n")
        with self.assertRaises(Exception):
            load_table().peak_rss(case)


FIXTURE_WINDOW = r'''
import sys, tkinter
mode = sys.argv[1]
root = tkinter.Tk()
root.title("command #1: fixture")
root.geometry("420x300+300+200")
if mode == "offdesktop":
    root.geometry("420x300-6000+200")
root.update()
if mode == "minimised":
    root.iconify()
    root.update()
root.after(5000, root.quit)
root.mainloop()
sys.exit(9)
'''


@unittest.skipUnless(sys.platform == "win32", "skipping: the native Windows capture needs Windows")
class Capture(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        try:
            import PIL  # noqa: F401
            import tkinter  # noqa: F401
        except ImportError:
            raise unittest.SkipTest("skipping: Pillow or tkinter is not installed (the capture tool needs them)")
        cls.tmp = Path(tempfile.mkdtemp(prefix="capture_contract_"))
        cls.window = cls.tmp / "fixture_window.py"
        cls.window.write_text(FIXTURE_WINDOW)

    def capture(self, mode):
        out = self.tmp / f"cap_{mode}"
        env = dict(os.environ, ANNO1503_GAME_DIR=str(self.tmp),
                   ANNO_CAPTURE_APP_ARGS=f'["{str(self.window).replace(chr(92), "/")}", "{mode}"]')
        p = subprocess.run([sys.executable, str(CAPTURE), sys.executable, "unused.script", str(out)],
                           env=env, capture_output=True, text=True, timeout=120)
        log = (out / "capture.log").read_text() if (out / "capture.log").is_file() else ""
        return p, out, log

    def test_control_a_window_on_the_desktop_is_captured(self):
        p, out, log = self.capture("normal")
        self.assertEqual(p.returncode, 9, p.stderr + log)  # the fixture's own status
        self.assertTrue((out / "shots" / "cmd1-client.png").is_file(), log)
        self.assertNotIn("REFUSED", log)

    def test_a_minimised_window_is_refused_and_no_picture_is_saved(self):
        p, out, log = self.capture("minimised")
        self.assertEqual(p.returncode, 9, p.stderr + log)
        self.assertIn("REFUSED, the window is minimised", log)
        self.assertEqual(list((out / "shots").glob("*.png")), [])

    def test_a_client_area_off_the_desktop_is_refused(self):
        p, out, log = self.capture("offdesktop")
        self.assertEqual(p.returncode, 9, p.stderr + log)
        self.assertIn("is not inside the desktop", log)
        self.assertEqual(list((out / "shots").glob("*.png")), [])


class SkipsAreNamed(unittest.TextTestResult):
    """Prints each skip as a line that starts with `skipping:`, the form the evidence gate
    (tools/test/ctest_verdict.py) counts as a skip and not as an executed pass."""

    def addSkip(self, test, reason):
        super().addSkip(test, reason)
        print(reason if reason.startswith("skipping") else "skipping: " + reason, flush=True)


if __name__ == "__main__":
    unittest.main(testRunner=unittest.TextTestRunner(resultclass=SkipsAreNamed))
