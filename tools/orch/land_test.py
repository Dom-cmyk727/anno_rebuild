"""The landing decision of tools/orch/land.py: which `gate` check run state lets a commit onto main.

    python -I tools/orch/land_test.py
"""
from __future__ import annotations

import subprocess
import sys
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import land  # noqa: E402
from land import gate_state  # noqa: E402


def cr(id_: int, status: str = "completed", conclusion: str | None = "success", name: str = "gate") -> dict:
    return {"id": id_, "name": name, "status": status, "conclusion": conclusion, "html_url": f"u{id_}"}


class GateState(unittest.TestCase):
    def test_no_run_is_absent(self):
        self.assertEqual(gate_state([])[0], "absent")

    def test_other_checks_do_not_count(self):
        self.assertEqual(gate_state([cr(1, name="cpp (linux-gcc)")])[0], "absent")

    def test_success_passes(self):
        self.assertEqual(gate_state([cr(1)]), ("success", cr(1)))

    def test_queued_and_running_are_pending(self):
        self.assertEqual(gate_state([cr(1, "queued", None)])[0], "pending")
        self.assertEqual(gate_state([cr(1, "in_progress", None)])[0], "pending")

    def test_only_success_passes(self):
        # GitHub itself would accept "skipped" and "neutral" for a required check; this gate must not.
        for conclusion in ("failure", "cancelled", "timed_out", "skipped", "neutral", "action_required", None):
            with self.subTest(conclusion=conclusion):
                self.assertEqual(gate_state([cr(1, conclusion=conclusion)])[0], "failure")

    def test_newest_run_decides(self):
        self.assertEqual(gate_state([cr(5, conclusion="failure"), cr(9)])[0], "success")
        self.assertEqual(gate_state([cr(9, conclusion="failure"), cr(5)])[0], "failure")
        self.assertEqual(gate_state([cr(5), cr(9, "in_progress", None)])[0], "pending")


class GateOf(unittest.TestCase):
    """gh's answers for a commit, as observed on 2026-11-10."""

    @staticmethod
    def answer(code: int, out: str, err: str = ""):
        return mock.patch.object(land, "run", return_value=subprocess.CompletedProcess([], code, out, err))

    def test_commit_github_has_not_seen_is_absent(self):
        out = '{"message":"No commit found for SHA: 7dcbebe0","status":"422"}'
        with self.answer(1, out, "gh: No commit found for SHA: 7dcbebe0 (HTTP 422)"):
            self.assertEqual(land.gate_of("o/r", "7dcbebe0"), ("absent", None))

    def test_other_api_errors_stop(self):
        with self.answer(1, '{"message":"Bad credentials","status":"401"}', "gh: Bad credentials (HTTP 401)"):
            with self.assertRaises(RuntimeError):
                land.gate_of("o/r", "7dcbebe0")

    def test_runs_are_read(self):
        with self.answer(0, '{"total_count":1,"check_runs":[{"id":3,"name":"gate","status":"completed",'
                            '"conclusion":"success","html_url":"u3"}]}'):
            self.assertEqual(land.gate_of("o/r", "7dcbebe0")[0], "success")


if __name__ == "__main__":
    unittest.main(verbosity=2)
