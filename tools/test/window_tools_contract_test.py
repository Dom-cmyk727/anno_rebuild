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
mode, code = sys.argv[1], int(sys.argv[2])
root = tkinter.Tk()
root.geometry("420x300+300+200")
if mode == "offdesktop":
    root.geometry("420x300-6000+200")
if mode != "silent":
    root.title("command #1: fixture")  # the capture tool asks for a picture when a title shows a receipt
root.update()
if mode == "minimised":
    root.iconify()
    root.update()
# "short": the window ends before the capture's 1.2 s delay is over; the others live 5 s.
root.after(600 if mode == "short" else 5000, root.quit)
root.mainloop()
sys.exit(code)
'''


@unittest.skipUnless(sys.platform == "win32", "skipping: the native Windows capture needs Windows")
class Capture(unittest.TestCase):
    '''Native: a real tiny window, the real capture tool, GDI grabs. The child's own status is chosen apart
    from the capture condition, so the aggregate verdict and the preserved child status are tested apart.'''

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

    def capture(self, mode, code=0, require=None):
        out = self.tmp / f"cap_{mode}_{code}_{require}"
        env = dict(os.environ, ANNO1503_GAME_DIR=str(self.tmp),
                   ANNO_CAPTURE_APP_ARGS=f'["{str(self.window).replace(chr(92), "/")}", "{mode}", "{code}"]')
        flags = ["--require-captures", str(require)] if require is not None else []
        p = subprocess.run([sys.executable, str(CAPTURE), *flags, sys.executable, "unused.script", str(out)],
                           env=env, capture_output=True, text=True, timeout=120)
        log = (out / "capture.log").read_text() if (out / "capture.log").is_file() else ""
        return p, out, log

    def test_control_a_window_on_the_desktop_is_captured_and_the_run_succeeds(self):
        p, out, log = self.capture("normal", code=0)
        self.assertEqual(p.returncode, 0, p.stderr + log)
        self.assertTrue((out / "shots" / "cmd1-client.png").is_file(), log)
        self.assertIn("capture_verdict=complete", log)
        self.assertIn("captures requested=1 completed=1 refused=0", log)
        self.assertNotIn("REFUSED", log)

    def test_the_child_status_is_preserved_when_the_captures_are_fine(self):
        p, out, log = self.capture("normal", code=9)
        self.assertEqual(p.returncode, 9, p.stderr + log)
        self.assertIn("window_exit=9", log)
        self.assertIn("capture_verdict=complete", log)

    def test_a_minimised_window_is_refused_and_the_run_fails_though_the_child_exited_0(self):
        p, out, log = self.capture("minimised", code=0)
        self.assertEqual(p.returncode, 4, p.stderr + log)
        self.assertIn("window_exit=0", log)
        self.assertIn("capture_verdict=INCOMPLETE", log)
        self.assertIn("refused=1", log)
        self.assertEqual(list((out / "shots").glob("*.png")), [])

    def test_a_client_area_off_the_desktop_is_refused_and_the_run_fails_though_the_child_exited_0(self):
        p, out, log = self.capture("offdesktop", code=0)
        self.assertEqual(p.returncode, 4, p.stderr + log)
        self.assertIn("is not inside the desktop", log)
        self.assertIn("capture_verdict=INCOMPLETE", log)
        self.assertEqual(list((out / "shots").glob("*.png")), [])

    def test_a_refusal_does_not_hide_a_failing_child(self):
        p, out, log = self.capture("minimised", code=9)
        self.assertEqual(p.returncode, 9, p.stderr + log)  # the child's own status stands
        self.assertIn("capture_verdict=INCOMPLETE", log)  # and the incomplete set is still reported

    def test_a_requested_capture_left_unfinished_when_the_window_ends_fails(self):
        p, out, log = self.capture("short", code=0)
        self.assertEqual(p.returncode, 4, p.stderr + log)
        self.assertIn("unfinished=1", log)
        self.assertEqual(list((out / "shots").glob("*.png")), [])

    def test_a_missing_required_capture_fails(self):
        p, out, log = self.capture("silent", code=0, require=1)
        self.assertEqual(p.returncode, 4, p.stderr + log)
        self.assertIn("requested=0 completed=0", log)
        self.assertIn("required=1", log)
        self.assertIn("capture_verdict=INCOMPLETE", log)


class CaptureMismatch(unittest.TestCase):
    '''The size-mismatch path of capture() itself (the grab is replaced by one of the wrong size; the
    window is a real tiny window of this process): no capture under a picture name, the invalid grab kept
    under a name no picture glob matches, result "mismatch".'''

    @classmethod
    def setUpClass(cls):
        if sys.platform != "win32":
            raise unittest.SkipTest("skipping: the capture tool imports Win32 and Pillow (Windows only)")
        try:
            import PIL  # noqa: F401
            import tkinter  # noqa: F401
        except ImportError:
            raise unittest.SkipTest("skipping: Pillow or tkinter is not installed (the capture tool needs them)")
        spec = importlib.util.spec_from_file_location("dev_session_capture_mismatch", CAPTURE)
        cls.mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.mod)

    def run_capture(self, grab):
        import tkinter
        from PIL import Image

        shots = Path(tempfile.mkdtemp(prefix="capture_mismatch_"))
        root = tkinter.Tk()
        root.geometry("420x300+300+200")
        root.update()
        self.addCleanup(root.destroy)
        hwnd = self.mod.user32.GetAncestor(root.winfo_id(), 2)  # GA_ROOT
        log = []
        real = self.mod.ImageGrab.grab
        self.mod.ImageGrab.grab = lambda bbox=None, all_screens=False: grab(bbox, Image)
        try:
            result = self.mod.capture(hwnd, "cmd1", shots, log)
        finally:
            self.mod.ImageGrab.grab = real
        return result, shots, log

    def test_control_a_grab_of_the_right_size_is_a_capture(self):
        result, shots, log = self.run_capture(lambda bbox, Image: Image.new("RGB", (bbox[2] - bbox[0], bbox[3] - bbox[1])))
        self.assertEqual(result, "ok", log)
        self.assertTrue((shots / "cmd1-client.png").is_file())
        self.assertEqual(list(shots.glob("*.invalid")), [])

    def test_a_grab_of_the_wrong_size_is_never_saved_as_a_capture(self):
        result, shots, log = self.run_capture(lambda bbox, Image: Image.new("RGB", (5, 5)))
        self.assertEqual(result, "mismatch", log)
        self.assertEqual(list(shots.glob("*.png")), [])
        self.assertTrue((shots / "cmd1-client.invalid").is_file())
        self.assertIn("GRAB SIZE MISMATCH", log[-1])


class CaptureVerdict(unittest.TestCase):
    '''The aggregate rule at its seam (no window, mocked results): the cases a desktop cannot make
    deterministically, such as a grab of the wrong size and a watchdog kill. Not native coverage.'''

    @classmethod
    def setUpClass(cls):
        if sys.platform != "win32":
            raise unittest.SkipTest("skipping: the capture tool imports Win32 and Pillow (Windows only)")
        try:
            import PIL  # noqa: F401
        except ImportError:
            raise unittest.SkipTest("skipping: Pillow is not installed (the capture tool imports it)")
        spec = importlib.util.spec_from_file_location("dev_session_capture_under_test", CAPTURE)
        cls.mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.mod)

    def test_all_requested_captures_done_is_success(self):
        code, lines = self.mod.verdict(0, ["1", "2"], {"1": "ok", "2": "ok"}, 2)
        self.assertEqual(code, 0)
        self.assertIn("capture_verdict=complete", lines)

    def test_a_size_mismatch_fails_with_a_clean_child(self):
        code, lines = self.mod.verdict(0, ["1"], {"1": "mismatch"}, 0)
        self.assertEqual(code, 4)
        self.assertTrue(any("size_mismatch=1" in line for line in lines))

    def test_an_unfinished_capture_and_too_few_captures_fail(self):
        self.assertEqual(self.mod.verdict(0, ["1", "2"], {"1": "ok"}, 0)[0], 4)
        self.assertEqual(self.mod.verdict(0, [], {}, 1)[0], 4)

    def test_a_failing_child_status_is_kept_and_a_watchdog_is_named(self):
        code, lines = self.mod.verdict(1, ["1"], {"1": "ok"}, 0, watchdog=True)
        self.assertEqual(code, 1)
        self.assertTrue(any("watchdog" in line for line in lines))


class SkipsAreNamed(unittest.TextTestResult):
    """Prints each skip as a line that starts with `skipping:`, the form the evidence gate
    (tools/test/ctest_verdict.py) counts as a skip and not as an executed pass."""

    def addSkip(self, test, reason):
        super().addSkip(test, reason)
        print(reason if reason.startswith("skipping") else "skipping: " + reason, flush=True)


if __name__ == "__main__":
    unittest.main(testRunner=unittest.TextTestRunner(resultclass=SkipsAreNamed))
