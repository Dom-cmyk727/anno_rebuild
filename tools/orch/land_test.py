"""The landing decision of tools/orch/land.py: which `gate` check runs let a commit onto main.

    python -I tools/orch/land_test.py
"""
from __future__ import annotations

import json
import subprocess
import sys
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import land  # noqa: E402
from land import gate_state  # noqa: E402

SHA = "0d11245705edd239d25d3bbc15ccb669853d7b7c"


def wf(path=".github/workflows/ci.yml", event="push", head_sha=SHA, status="completed") -> dict:
    return {"path": path, "event": event, "head_sha": head_sha, "status": status}


def cr(id_: int, status: str = "completed", conclusion: str | None = "success", name: str = "gate",
       workflow: dict | None = None) -> dict:
    return {"id": id_, "name": name, "status": status, "conclusion": conclusion, "html_url": f"u{id_}",
            "workflow": wf() if workflow is None else workflow}


class GateState(unittest.TestCase):
    def test_no_run_is_absent(self):
        self.assertEqual(gate_state([], SHA)[0], "absent")

    def test_other_checks_do_not_count(self):
        self.assertEqual(gate_state([cr(1, name="cpp (linux-gcc)")], SHA)[0], "absent")

    def test_success_passes(self):
        self.assertEqual(gate_state([cr(1)], SHA), ("success", cr(1)))

    def test_queued_and_running_are_pending(self):
        self.assertEqual(gate_state([cr(1, "queued", None)], SHA)[0], "pending")
        self.assertEqual(gate_state([cr(1, "in_progress", None)], SHA)[0], "pending")

    def test_only_success_passes(self):
        # GitHub itself would accept "skipped" and "neutral" for a required check; this gate must not.
        for conclusion in ("failure", "cancelled", "timed_out", "skipped", "neutral", "action_required", None):
            with self.subTest(conclusion=conclusion):
                self.assertEqual(gate_state([cr(1, conclusion=conclusion)], SHA)[0], "failure")

    def test_newest_run_decides(self):
        self.assertEqual(gate_state([cr(5, conclusion="failure"), cr(9)], SHA)[0], "success")
        self.assertEqual(gate_state([cr(9, conclusion="failure"), cr(5)], SHA)[0], "failure")
        self.assertEqual(gate_state([cr(5), cr(9, "in_progress", None)], SHA)[0], "pending")

    def test_a_failure_whose_run_is_rerunning_is_pending(self):
        # `gh run rerun --failed`: the run is in progress again, its new gate appears only when the jobs are done.
        self.assertEqual(gate_state([cr(5, conclusion="failure", workflow=wf(status="in_progress"))], SHA)[0],
                         "pending")
        self.assertEqual(gate_state([cr(5, conclusion="failure", workflow=wf(status="queued"))], SHA)[0], "pending")

    def test_only_a_ci_yml_push_of_this_commit_counts(self):
        # A pull request's gate may compare against another branch; another workflow or commit proves nothing.
        for label, other in (("pull request", wf(event="pull_request")),
                             ("other workflow", wf(path=".github/workflows/evil.yml")),
                             ("other commit", wf(head_sha="1" * 40)),
                             ("no workflow run found", {})):
            with self.subTest(label):
                self.assertEqual(gate_state([cr(7, workflow=other)], SHA)[0], "absent")
                # ... and it does not outvote a counting failure either.
                self.assertEqual(gate_state([cr(3, conclusion="failure"), cr(7, workflow=other)], SHA)[0], "failure")


class GateOf(unittest.TestCase):
    """gh's answers, shaped as observed on 2026-11-10 (runs 42883373191 and 42883466787)."""

    @staticmethod
    def answers(*replies):
        return mock.patch.object(land, "run", side_effect=[subprocess.CompletedProcess([], c, o, e) for c, o, e in replies])

    def test_commit_github_has_not_seen_is_absent(self):
        out = '{"message":"No commit found for SHA: 0d112457","status":"422"}'
        with self.answers((1, out, "gh: No commit found for SHA: 0d112457 (HTTP 422)")):
            self.assertEqual(land.gate_of("o/r", SHA), ("absent", None))

    def test_other_api_errors_stop(self):
        with self.answers((1, '{"message":"Bad credentials","status":"401"}', "gh: Bad credentials (HTTP 401)")):
            with self.assertRaises(RuntimeError):
                land.gate_of("o/r", SHA)

    def test_check_run_is_matched_to_its_workflow_run(self):
        check_runs = {"total_count": 1, "check_runs": [
            {"id": 3, "name": "gate", "status": "completed", "conclusion": "success", "html_url": "u3",
             "check_suite": {"id": 77}}]}
        for run_json, want in ((wf(), "success"), (wf(event="pull_request"), "absent")):
            with self.subTest(event=run_json["event"]):
                with self.answers((0, json.dumps(check_runs), ""),
                                  (0, json.dumps({"total_count": 1, "workflow_runs": [run_json]}), "")):
                    self.assertEqual(land.gate_of("o/r", SHA)[0], want)


if __name__ == "__main__":
    unittest.main(verbosity=2)
