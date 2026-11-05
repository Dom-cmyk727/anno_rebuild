#!/usr/bin/env python3
"""Public-command contract of tools/test/viewer_wiring.py (W4-WIN-FINAL F1, W4-WIN-VERIFY V1 and V2): a child that
exits 0 is not a passing session unless the session record shows the invoked script ran, in full, and the child
wrote no sanitizer report on either stream. The command is run as a person runs it, against a tiny fixture
program that stands in for the window and writes (or withholds) the session record and its diagnostics; no game
files and no window. Every broken-evidence case is preceded by the positive control in the same class: the
complete, matching record passes.

The script identities below were derived apart from the code under test: FNV-1a 64 (offset basis
cbf29ce484222325, prime 100000001b3) over one `LINE VERB ARG...` line per step after comments and whitespace,
wait steps included (tiny.script: "2 look 1 2", "3 key b", "4 wait frames 2", "5 checkpoint a", "6 quit").

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

# What the window records for tiny.script in the real format (see viewer_session.py): the identity of the
# parsed script first, `ran line` with every argument for each non-wait step in order, a `checkpoint` line
# before its `ran line`.
ID_TINY = "script identity: steps=5 fnv1a64=c0292a5964fabae3"
GOOD = [
    ID_TINY,
    "ran line 2: look 1 2",
    "ran line 3: key b",
    "checkpoint a: turn 1, frames 2, outcome none, visibility NoVisualChange, title: x",
    "ran line 5: checkpoint a",
    "ran line 6: quit",
]
MODES = {
    "complete": (GOOD, 0),
    "empty": ([], 0),
    "truncated": (GOOD[:-1], 0),
    "missing-middle": (GOOD[:2] + GOOD[3:], 0),
    "reordered": ([GOOD[0], GOOD[2], GOOD[1]] + GOOD[3:], 0),
    "wrong-script": ([ID_TINY, "ran line 2: look 9", "ran line 3: click 1", "ran line 4: quit"], 0),
    "extra-record": (GOOD + ["ran line 7: key n"], 0),
    "failed-step": (GOOD[:3] + ["FAIL line 3 (key): unknown key"] + GOOD[3:], 0),
    "child-fails": (GOOD, 7),
    "no-session-txt": (GOOD, 0),
    "noquit": (["script identity: steps=1 fnv1a64=2560f975e57dccf0", "ran line 1: look 1 2"], 0),
}
# What an earlier window recorded for tiny.script (W4-WIN-FINAL): no identity line, first argument only.
LEGACY = ["ran line 2: look 1", "ran line 3: key b", GOOD[3], "ran line 5: checkpoint a", "ran line 6: quit"]

sys.path.insert(0, str(HERE / "tools" / "test"))  # -I leaves the script's directory off the path
import ctest_verdict  # noqa: E402
import viewer_session  # noqa: E402

UBSAN = "fixture.cpp:42:7: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'"
ASAN = "==4242==ERROR: AddressSanitizer: heap-use-after-free on address 0x602000000010 at pc 0x0001 bp 0x0002"
SDL = ["CPU frame delivery failed (SDL_UpdateTexture: Invalid texture) ...",
       "CPU frame delivery failed (SDL_RenderTexture: Invalid texture) ...",
       "CPU frame delivery failed (SDL_RenderPresent: Invalid renderer) ..."]
SDL_ARGS = [a for needle in ("SDL_UpdateTexture", "SDL_RenderTexture", "SDL_RenderPresent")
            for a in ("--expect-stderr", f"CPU frame delivery failed ({needle}")]

FIXTURE = r"""
import os, sys, time
from pathlib import Path
mode = os.environ["FIXTURE_MODE"]
session = Path(os.environ["ANNO_DEV_SESSION"])
if mode == "hang":
    time.sleep(60)
sys.stdout.write(os.environ.get("FIXTURE_STDOUT", ""))
sys.stderr.write(os.environ.get("FIXTURE_STDERR", ""))
sys.stdout.flush()
sys.stderr.flush()
if mode == "hang-late":
    time.sleep(60)
lines, code = eval(os.environ["FIXTURE_RECORD"])
if mode != "absent":
    session.mkdir(parents=True)
    # Bytes as the window writes them (LF, any argument byte); surrogate escapes stand for non-UTF-8 bytes.
    (session / "script.log").write_bytes(("\n".join(lines) + ("\n" if lines else "")).encode("utf-8", "surrogateescape"))
    (session / "child_ubsan_options.txt").write_text(os.environ.get("UBSAN_OPTIONS", "<unset>"), encoding="utf-8")
    (session / "child_argv.txt").write_text("\n".join(sys.argv[1:]), encoding="utf-8")
    (session / "child_env.txt").write_text(os.environ.get("ANNO_SAVE_DIR", "<unset>") + "|" + os.environ.get("ANNO_VIEW_AT", "<unset>"), encoding="utf-8")
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
            cls.exe.write_text(f'@echo off\r\n"{sys.executable}" "{cls.tmp / "fixture.py"}" %*\r\nexit /b %ERRORLEVEL%\r\n')
        else:
            cls.exe = cls.tmp / "window.sh"
            cls.exe.write_text(f'#!/bin/sh\nexec "{sys.executable}" "{cls.tmp / "fixture.py"}" "$@"\n')
            cls.exe.chmod(cls.exe.stat().st_mode | stat.S_IXUSR)
        cls.n = 0

    def variant(self, name, text):
        path = self.tmp / f"{name}.script"
        path.write_bytes(text.encode("utf-8"))
        return path

    def run_case(self, mode, extra_env=None, script=TINY, record=None, code=None, stdout="", stderr="", args=()):
        type(self).n += 1
        out = self.tmp / f"session{self.n}"
        lines, status = MODES.get(mode, ([], 0))
        if record is not None:
            lines = record
        if code is not None:
            status = code
        env = dict(os.environ, ANNO1503_GAME_DIR=str(self.tmp), FIXTURE_MODE=mode,
                   FIXTURE_RECORD=repr((lines, status)), FIXTURE_STDOUT=stdout, FIXTURE_STDERR=stderr)
        env.pop("UBSAN_OPTIONS", None)
        env.update(extra_env or {})
        p = subprocess.run([sys.executable, "-I", str(WIRING), str(self.exe), str(script), str(out), *args],
                           env=env, capture_output=True, text=True, timeout=120)
        return p, out

    def expect_failure(self, mode, fragment, extra_env=None, **kw):
        p, out = self.run_case(mode, extra_env, **kw)
        self.assertEqual(p.returncode, 1, p.stdout + p.stderr)
        self.assertNotIn("PASS", p.stdout)
        self.assertIn(fragment, p.stdout, p.stdout)
        return p, out

    def expect_pass(self, mode="complete", **kw):
        p, out = self.run_case(mode, **kw)
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertIn("PASS tiny" if kw.get("script", TINY) == TINY else "PASS", p.stdout)
        return p, out

    # ---- W4-OPENING-1: another start than lan5 is launched with the caller's own arguments
    def test_app_args_replace_the_default_command_line(self):
        _, out = self.expect_pass(args=("--app-arg", "--ship-start", "--app-arg", "m.json", "--app-arg", "--seat",
                                        "--app-arg", "0"))
        argv = (out / "child_argv.txt").read_text(encoding="utf-8").split("\n")
        self.assertEqual(argv, ["--ship-start", "m.json", "--seat", "0", "--renderer", "cpu"])
        save_dir, view_at = (out / "child_env.txt").read_text(encoding="utf-8").split("|")
        self.assertTrue(save_dir.endswith(f"{out.name}.saves"), save_dir)
        self.assertEqual(view_at, "<unset>")  # the lan5 camera hook is not given to another start

    def test_without_app_args_the_default_is_unchanged(self):
        _, out = self.expect_pass()
        argv = (out / "child_argv.txt").read_text(encoding="utf-8").split("\n")
        self.assertEqual(argv[0], "--play")
        self.assertTrue(argv[1].endswith("lan5.txt"), argv)
        self.assertEqual(argv[2:], ["--renderer", "cpu"])
        self.assertEqual((out / "child_env.txt").read_text(encoding="utf-8").split("|")[1], "1140,516")

    # ---- F1: the session record shows the invoked script ran

    def test_control_a_complete_matching_record_passes(self):
        p, out = self.expect_pass()
        self.assertTrue((out / "script.log").is_file())

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
        noquit = self.variant("noquit", "look 1 2\n")
        self.expect_failure("noquit", "does not end with quit", script=noquit)

    def test_a_missing_game_dir_is_a_named_skip_not_a_pass(self):
        env = {k: v for k, v in os.environ.items() if k != "ANNO1503_GAME_DIR"}
        p = subprocess.run([sys.executable, "-I", str(WIRING), str(self.exe), str(TINY), str(self.tmp / "nogame")],
                           env=env, capture_output=True, text=True, timeout=60)
        self.assertEqual(p.returncode, 77, p.stdout + p.stderr)
        self.assertIn("skipping: ANNO1503_GAME_DIR is not set", p.stdout)

    # ---- V2: the record identifies the whole script the window parsed

    def test_control_a_variant_script_with_its_own_complete_record_passes(self):
        # The three variants below pass with the record their own script leaves; only the difference between the
        # invoked script and the recorded one is what fails them.
        look = self.variant("look999", TINY.read_text().replace("look 1 2", "look 1 999"))
        self.expect_pass(script=look, record=["script identity: steps=5 fnv1a64=f7c26d34e43afa92"] + [
            "ran line 2: look 1 999"] + GOOD[2:])
        wait = self.variant("wait3", TINY.read_text().replace("wait frames 2", "wait frames 3"))
        self.expect_pass(script=wait, record=["script identity: steps=5 fnv1a64=acbcaeec0048948e"] + GOOD[1:])
        expect2 = self.variant("expect2", "# c\nlook 1 2\nexpect work rebuilds == 2\nquit\n")
        self.expect_pass(script=expect2, record=["script identity: steps=3 fnv1a64=d87cd9b57110c259",
                                                 "ran line 2: look 1 2", "ran line 3: expect work rebuilds == 2",
                                                 "ran line 4: quit"])

    def test_a_changed_second_coordinate_does_not_pass(self):
        # The review's counterexample: the log of `look 1 2` against an invoked `look 1 999`.
        look = self.variant("look999", TINY.read_text().replace("look 1 2", "look 1 999"))
        self.expect_failure("complete", "script identity mismatch", script=look)
        # Isolated: the trace is the invoked script's, only the recorded identity is the old script's.
        self.expect_failure("complete", "script identity mismatch", script=look,
                            record=[ID_TINY, "ran line 2: look 1 999"] + GOOD[2:])

    def test_a_changed_wait_value_does_not_pass(self):
        # A wait leaves no ran record, so the trace of the two scripts is the same: only the identity differs.
        wait = self.variant("wait3", TINY.read_text().replace("wait frames 2", "wait frames 3"))
        p, _ = self.expect_failure("complete", "script identity mismatch", script=wait)
        self.assertNotIn("the script's step is", p.stdout)

    def test_a_changed_later_argument_of_an_expectation_does_not_pass(self):
        expect2 = self.variant("expect2", "# c\nlook 1 2\nexpect work rebuilds == 2\nquit\n")
        expect1 = ["script identity: steps=3 fnv1a64=eb94f929f4d1862c", "ran line 2: look 1 2",
                   "ran line 3: expect work rebuilds == 1", "ran line 4: quit"]
        self.expect_failure("complete", "script identity mismatch", script=expect2, record=expect1)
        isolated = [expect1[0], "ran line 2: look 1 2", "ran line 3: expect work rebuilds == 2", "ran line 4: quit"]
        self.expect_failure("complete", "script identity mismatch", script=expect2, record=isolated)

    def test_a_later_argument_in_a_ran_record_is_compared_not_only_the_identity(self):
        # The identity is the invoked script's own here; the trace claims another later argument / coordinate.
        expect2 = self.variant("expect2", "# c\nlook 1 2\nexpect work rebuilds == 2\nquit\n")
        self.expect_failure("complete", "record 2 is", script=expect2,
                            record=["script identity: steps=3 fnv1a64=d87cd9b57110c259", "ran line 2: look 1 2",
                                    "ran line 3: expect work rebuilds == 1", "ran line 4: quit"])
        self.expect_failure("complete", "record 1 is", record=[ID_TINY, "ran line 2: look 1 999"] + GOOD[2:])

    def test_the_python_identity_agrees_with_the_cpp_vectors(self):
        # The same scripts and constants as the C++ case `the log opens with the identity` (dev_script_test.cpp),
        # which hashes them with the window's own parser: both implementations are held to one value each.
        vectors = [
            (b"# comment\nexpect title-lacks a  b # trailing\n\twait frames 3\nquit\n", "8d702b850fad24d4", 3),
            (b"# other\nexpect   title-lacks a b\nwait\tframes 3  # x\nquit", "8d702b850fad24d4", 3),
            (b"# comment\nexpect title-lacks a c\nwait frames 3\nquit\n", "6d327550d5e6e183", 3),
            (b"# comment\nexpect title-lacks a b\nwait frames 4\nquit\n", "7f40b977c080e275", 3),
            (TINY.read_bytes(), "c0292a5964fabae3", 5),
            (b"note a\xc2\x85b\nnote c\xe2\x80\xa8d\nquit\n", "7c2889893a0c95db", 3),
        ]
        for script, digest, steps in vectors:
            with self.subTest(script=script[:40]):
                self.assertEqual(viewer_session.identity_line(script),
                                 f"script identity: steps={steps} fnv1a64={digest}")

    def test_arguments_with_exotic_bytes_are_compared_byte_for_byte(self):
        # U+0085 and U+2028 are not whitespace to the window's parser and are not line ends of its log.
        script = self.variant("exotic", "note a\u0085b\nnote c d\nquit\n")
        identity = viewer_session.identity_line(script.read_bytes())
        self.expect_pass(script=script, record=[identity, "ran line 1: note a\u0085b", "ran line 2: note c d",
                                                "ran line 3: quit"])

    def test_a_non_utf8_argument_byte_is_compared_byte_for_byte_and_printed_safely(self):
        path = self.tmp / "ff.script"
        path.write_bytes(b"note a\xff\xfeb\nquit\n")
        identity = viewer_session.identity_line(path.read_bytes())
        p, _ = self.expect_pass(script=path, record=[identity, "ran line 1: note a\udcff\udcfeb", "ran line 2: quit"])
        self.assertIn("\\udcff", p.stdout)  # the echoed record is printed with a backslash escape, not an exception

    def test_a_log_copied_in_text_mode_with_crlf_is_the_same_record(self):
        script = TINY.read_bytes()
        text = "\r\n".join(GOOD) + "\r\n"
        self.assertEqual(viewer_session.problems(script, text), [])

    def test_a_missing_identity_still_reports_a_failed_step(self):
        found = viewer_session.problems(TINY.read_bytes(), "\n".join(LEGACY[:1] + ["FAIL line 3 (key): boom"]) + "\n")
        self.assertTrue(any(f.startswith("a step failed: FAIL line 3") for f in found), found)
        self.assertTrue(any("no `script identity:` line" in f for f in found), found)

    def test_revalidation_marks_a_legacy_log_that_several_scripts_fit_as_ambiguous(self):
        scripts = self.tmp / "reval_ambiguous"
        scripts.mkdir(exist_ok=True)
        (scripts / "a.script").write_bytes(TINY.read_bytes())
        (scripts / "b.script").write_bytes(TINY.read_bytes().replace(b"look 1 2", b"look 1 999"))
        d = self.tmp / "reval_ambiguous_session"
        d.mkdir(exist_ok=True)
        (d / "script.log").write_text("\n".join(LEGACY) + "\n", encoding="utf-8")
        p = subprocess.run([sys.executable, "-I", str(HERE / "tools" / "test" / "viewer_session.py"),
                            "--revalidate", str(scripts), str(d)], capture_output=True, text=True, timeout=60)
        self.assertEqual(p.returncode, 0, p.stdout)  # a legacy log is not an error, even an ambiguous one
        self.assertIn("a.script, b.script", p.stdout)
        self.assertIn("AMBIGUOUS", p.stdout)
        self.assertTrue(p.stdout.startswith("LEGACY"), p.stdout)

    def test_a_legacy_record_without_the_identity_line_does_not_pass(self):
        # An earlier window's log (first argument only) matches the old reduced comparison; it is not a verified
        # full identity, so the wrapper refuses it and says why.
        self.expect_failure("complete", "no `script identity:` line", record=LEGACY)

    def test_the_identity_line_must_open_the_record_once(self):
        self.expect_failure("complete", "must open with exactly one", record=GOOD[1:2] + [ID_TINY] + GOOD[2:])
        self.expect_failure("complete", "must open with exactly one", record=[ID_TINY] + GOOD)

    # ---- V1: child diagnostics are part of the verdict

    def test_control_expected_sdl_diagnostics_alone_pass(self):
        # The F2 case's positive evidence: the window must write these, and they are not sanitizer reports.
        self.expect_pass(stderr="\n".join(SDL) + "\n", args=SDL_ARGS)

    def test_a_ubsan_report_on_stderr_fails_although_the_child_exits_zero(self):
        p, out = self.expect_failure("complete", "sanitizer diagnostics", stderr=UBSAN + "\n")
        self.assertIn("runtime error: signed integer overflow", p.stdout)
        self.assertIn("stderr.txt:1:", p.stdout)  # the artifact path and line reach the gate-visible output
        self.assertTrue(Path(f"{out}.stderr.txt").is_file())

    def test_an_asan_report_on_stderr_fails_although_the_child_exits_zero(self):
        p, _ = self.expect_failure("complete", "sanitizer diagnostics", stderr="note\n" + ASAN + "\n")
        self.assertIn("AddressSanitizer: heap-use-after-free", p.stdout)
        self.assertIn("stderr.txt:2:", p.stdout)

    def test_a_sanitizer_report_on_stdout_fails_too(self):
        p, out = self.expect_failure("complete", "sanitizer diagnostics", stdout=UBSAN + "\n")
        self.assertIn("stdout.txt:1:", p.stdout)
        self.assertTrue(Path(f"{out}.stdout.txt").is_file())  # stdout is kept, not discarded

    def test_each_remaining_sanitizer_kind_fails(self):
        for text in ("==1==ERROR: LeakSanitizer: detected memory leaks", "WARNING: ThreadSanitizer: data race (pid=1)",
                     "SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior fixture.cpp:1:1"):
            with self.subTest(text=text):
                self.expect_failure("complete", "sanitizer diagnostics", stderr=text + "\n")

    def test_a_sanitizer_report_beside_the_expected_sdl_diagnostics_still_fails(self):
        p, _ = self.expect_failure("complete", "sanitizer diagnostics", stderr="\n".join(SDL) + "\n" + UBSAN + "\n",
                                   args=SDL_ARGS)
        self.assertNotIn("lacks the expected diagnostic", p.stdout)

    def test_a_sanitizer_report_and_a_failing_child_are_both_reported(self):
        p, _ = self.expect_failure("child-fails", "window exit 7", stderr=UBSAN + "\n")
        self.assertIn("runtime error: signed integer overflow", p.stdout)

    def test_a_missing_expected_sdl_diagnostic_still_fails(self):
        self.expect_failure("complete", "lacks the expected diagnostic", stderr=SDL[0] + "\n", args=SDL_ARGS)

    def test_a_long_line_is_cut_around_the_report_so_the_gate_still_counts_it(self):
        long_line = "x" * 700 + " " + UBSAN + " " + "y" * 700
        p, _ = self.expect_failure("complete", "sanitizer diagnostics", stderr=long_line + "\n")
        printed = [line for line in p.stdout.splitlines() if "stderr.txt:1:" in line]
        self.assertEqual(len(printed), 1, p.stdout)
        self.assertIsNotNone(ctest_verdict.SANITIZER.search(printed[0]))  # what the canonical reader would count
        self.assertLess(len(printed[0]), 800)

    def test_a_sanitizer_report_beside_a_hung_window_names_the_watchdog_and_the_report(self):
        p, _ = self.expect_failure("hang-late", "window exit watchdog", {"ANNO_VIEWER_WIRING_WATCHDOG_S": "2"},
                                   stderr=UBSAN + "\n")
        self.assertIn("runtime error: signed integer overflow", p.stdout)

    def test_the_session_log_is_echoed_even_when_a_report_fails_the_case(self):
        p, _ = self.expect_failure("complete", "sanitizer diagnostics", stderr=UBSAN + "\n")
        self.assertIn("ran line 6: quit", p.stdout)

    def test_a_caller_option_given_with_env_wins_over_the_ubsan_default(self):
        _, out = self.expect_pass(args=("--env", "UBSAN_OPTIONS=print_stacktrace=0"))
        self.assertEqual((out / "child_ubsan_options.txt").read_text(), "print_stacktrace=0")

    def test_ubsan_is_made_fatal_unless_the_caller_chose_its_options(self):
        _, out = self.expect_pass()
        self.assertEqual((out / "child_ubsan_options.txt").read_text(), "halt_on_error=1:print_stacktrace=1")
        _, out = self.expect_pass(extra_env={"UBSAN_OPTIONS": "print_stacktrace=0"})
        self.assertEqual((out / "child_ubsan_options.txt").read_text(), "print_stacktrace=0")

    # ---- revalidating retained sessions says exactly which checks ran

    def test_revalidation_names_a_legacy_log_and_does_not_verify_its_full_identity(self):
        scripts = self.tmp / "reval_scripts"
        scripts.mkdir(exist_ok=True)
        (scripts / "tiny.script").write_bytes(TINY.read_bytes())
        sessions = []
        for name, record in (("full", GOOD), ("legacy", LEGACY),
                             ("other-coordinate", ["script identity: steps=5 fnv1a64=f7c26d34e43afa92",
                                                   "ran line 2: look 1 999"] + GOOD[2:])):
            d = self.tmp / f"reval_{name}"
            d.mkdir(exist_ok=True)
            (d / "script.log").write_text("\n".join(record) + "\n", encoding="utf-8")
            sessions.append(str(d))
        p = subprocess.run([sys.executable, "-I", str(HERE / "tools" / "test" / "viewer_session.py"),
                            "--revalidate", str(scripts), *sessions], capture_output=True, text=True, timeout=60)
        by_name = {Path(line.split()[1].rstrip(":")).name: line for line in p.stdout.splitlines() if line.strip()}
        self.assertTrue(by_name["reval_full"].startswith("VALID"), p.stdout)
        self.assertIn("full script identity", by_name["reval_full"])
        self.assertTrue(by_name["reval_legacy"].startswith("LEGACY"), p.stdout)
        self.assertIn("not verified", by_name["reval_legacy"])
        self.assertTrue(by_name["reval_other-coordinate"].startswith("INVALID"), p.stdout)
        self.assertEqual(p.returncode, 1)  # the invalid one decides; a legacy log alone is not an error


if __name__ == "__main__":
    unittest.main()
