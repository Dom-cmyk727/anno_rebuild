#!/usr/bin/env python3
"""Public-command contracts for the read-only recovery report."""
import pathlib
import subprocess
import sys
import tempfile
import unittest

TOOL = pathlib.Path(__file__).with_name("journal_report.py")


class Contract(unittest.TestCase):
    def invoke(self, data):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "journal"
            path.write_bytes(data)
            result = subprocess.run([sys.executable, "-I", str(TOOL), str(path)],
                                    capture_output=True, text=True, timeout=5)
            self.assertEqual(path.read_bytes(), data)
            self.assertEqual(sorted(p.name for p in path.parent.iterdir()), ["journal"])
            return result

    def test_open_promises_include_real_owners(self):
        out = self.invoke(b"A 1 7 50 3 123 456 0\nC 2 3 789 101\nC 2 3 789 101\nD 2 3 789 101\n")
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertIn("1 open memory holds, 1 open CPU tokens", out.stdout)
        self.assertIn("hold=7 bytes=50 namespace=3 pid=123 started=456", out.stdout)
        self.assertIn("count=1 namespace=3 pid=789 started=101", out.stdout)
        self.assertIn("owner liveness is unverified", out.stdout)

    def test_complete_releases_and_replaced_hold(self):
        out = self.invoke(b"A 1 7 50 3 123 456 0\nA 1 7 80 3 124 457 0\nR 1 7\nC 2 3 789 101\nD 2 3 789 101\nD 2 3 789 101\n")
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertIn("0 open memory holds, 0 open CPU tokens", out.stdout)

    def test_truncated_release_is_not_a_clean_report(self):
        out = self.invoke(b"A 1 7 50 3 123 456 0\nR 1 7")
        self.assertEqual(out.returncode, 1)
        self.assertIn("newline-terminated", out.stderr)
        self.assertNotIn("0 open memory", out.stdout)

    def test_bad_records_fail_closed(self):
        for data in (b"garbage\n", b"A 1 2\n", b"R 1 7 extra\n", b"R -1 7\n",
                     b"R 18446744073709551616 7\n", b"C 1 4294967296 2 3\n",
                     b"R 1 7\0\n", b"R 1 \xff\n", b"R 1 " + b"7" * 512 + b"\n"):
            with self.subTest(data=data[:40]):
                out = self.invoke(data)
                self.assertEqual(out.returncode, 1)
                self.assertIn("UNPROVABLE", out.stderr)
                self.assertNotIn("open memory holds", out.stdout)

    def test_empty_and_unsigned_limits(self):
        for data in (b"", b" \t\r\n", b"R 18446744073709551615 18446744073709551615\n"):
            out = self.invoke(data)
            self.assertEqual(out.returncode, 0, out.stderr)
            self.assertIn("0 open memory holds, 0 open CPU tokens", out.stdout)

    def test_unreadable_and_missing_argument(self):
        with tempfile.TemporaryDirectory() as directory:
            out = subprocess.run([sys.executable, "-I", str(TOOL), directory], capture_output=True, text=True, timeout=5)
            self.assertEqual(out.returncode, 1)
            self.assertIn("UNPROVABLE", out.stderr)
        out = subprocess.run([sys.executable, "-I", str(TOOL)], capture_output=True, text=True, timeout=5)
        self.assertEqual(out.returncode, 2)


if __name__ == "__main__":
    unittest.main()
