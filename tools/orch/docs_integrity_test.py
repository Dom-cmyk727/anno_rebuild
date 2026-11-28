"""Self-test of tools/orch/docs_integrity.py (HARNESS-GUARD-1).

    python -I tools/orch/docs_integrity_test.py
"""
from __future__ import annotations

import contextlib
import io
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import docs_integrity as di  # noqa: E402


class Repo:
    """A throwaway git repo in a temp dir, holding an AGENTS.md that links (CLAUDE.md) by default."""

    def __init__(self, path: str):
        self.path = path
        self._git("init", "-q")
        self._git("config", "user.email", "t@t")
        self._git("config", "user.name", "t")

    def _git(self, *args: str) -> str:
        return subprocess.run(["git", "-C", self.path, *args], capture_output=True, text=True,
                              check=True).stdout

    def write(self, rel: str, content: str) -> None:
        p = Path(self.path) / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(content, encoding="utf-8")

    def write_bytes(self, rel: str, data: bytes) -> None:
        p = Path(self.path) / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)

    def commit(self, message: str) -> str:
        self._git("add", "-A")
        self._git("commit", "-q", "-m", message)
        return self._git("rev-parse", "HEAD").strip()


def _lines(prefix: str, n: int) -> str:
    return "".join(f"{prefix}-{i}\n" for i in range(n))


class DeletionGuardBoundary(unittest.TestCase):
    def _case(self, added: int, deleted: int) -> list[str]:
        with tempfile.TemporaryDirectory() as d:
            r = Repo(d)
            r.write("AGENTS.md", "(CLAUDE.md)\n")
            r.write("big.txt", _lines("b", deleted))
            base = r.commit("base")
            r.write("big.txt", _lines("h", added))
            head = r.commit("head")
            findings, _ = di.check(d, base, head, lint=False)
            return findings

    def test_just_under_floor(self):
        self.assertEqual(self._case(0, 199), [])

    def test_at_floor(self):
        self.assertEqual(self._case(0, 200), ["deletion: big.txt +0 -200"])

    def test_ratio_fails_floor_passes(self):
        self.assertEqual(self._case(100, 300), [])

    def test_both_pass(self):
        self.assertEqual(self._case(10, 210), ["deletion: big.txt +10 -210"])


class BinaryFile(unittest.TestCase):
    def test_binary_skipped(self):
        with tempfile.TemporaryDirectory() as d:
            r = Repo(d)
            r.write("AGENTS.md", "(CLAUDE.md)\n")
            r.write_bytes("img.bin", b"\x00" * 500)
            base = r.commit("base")
            r.write_bytes("img.bin", b"\x00")
            head = r.commit("head")
            findings, _ = di.check(d, base, head, lint=False)
            self.assertEqual(findings, [])


class PathEdgeCases(unittest.TestCase):
    def test_space_dash_nonascii_paths(self):
        with tempfile.TemporaryDirectory() as d:
            r = Repo(d)
            r.write("AGENTS.md", "(CLAUDE.md)\n")
            paths = ["a b.txt", "a - b.txt", "ünïcode.txt"]
            for p in paths:
                r.write(p, _lines("b", 250))
            base = r.commit("base")
            for p in paths:
                r.write(p, "")
            head = r.commit("head")
            findings, _ = di.check(d, base, head, lint=False)
            for p in paths:
                self.assertIn(f"deletion: {p} +0 -250", findings)


class Trailers(unittest.TestCase):
    def _setup(self, message: str) -> tuple[str, str, str]:
        d = tempfile.mkdtemp()
        r = Repo(d)
        r.write("AGENTS.md", "(CLAUDE.md)\n")
        r.write("doc.md", _lines("b", 250))
        base = r.commit("base")
        r.write("doc.md", "")
        r._git("add", "-A")
        r._git("commit", "-q", "-m", message)
        head = r._git("rev-parse", "HEAD").strip()
        return d, base, head

    def test_matching_trailer_honoured(self):
        d, base, head = self._setup("cut doc.md\n\nLarge-Deletion: doc.md - archived to history\n")
        findings, honoured = di.check(d, base, head, lint=False)
        self.assertEqual(findings, [])
        self.assertEqual(honoured, ["trailer: doc.md - archived to history"])

    def test_trailer_names_another_path_not_honoured(self):
        d, base, head = self._setup("cut doc.md\n\nLarge-Deletion: other.md - archived\n")
        findings, honoured = di.check(d, base, head, lint=False)
        self.assertEqual(findings, ["deletion: doc.md +0 -250"])
        self.assertEqual(honoured, [])

    def test_trailer_empty_reason_not_honoured(self):
        d, base, head = self._setup("cut doc.md\n\nLarge-Deletion: doc.md -\n")
        findings, honoured = di.check(d, base, head, lint=False)
        self.assertEqual(findings, ["deletion: doc.md +0 -250"])
        self.assertEqual(honoured, [])

    def test_trailer_mid_body_ignored(self):
        # Git reads trailers only from a message's last paragraph; one in an earlier paragraph
        # (followed by a second paragraph) is not a trailer at all.
        d, base, head = self._setup("Large-Deletion: doc.md - archived\n\nmore text follows\n")
        findings, honoured = di.check(d, base, head, lint=False)
        self.assertEqual(findings, ["deletion: doc.md +0 -250"])
        self.assertEqual(honoured, [])


class Lint(unittest.TestCase):
    def _mk(self) -> tuple[str, Repo]:
        d = tempfile.mkdtemp()
        return d, Repo(d)

    def test_banned_phrase_use_a_review_subagent_fails(self):
        d, r = self._mk()
        r.write("AGENTS.md", "(CLAUDE.md)\nUse a review subagent for everything.\n")
        head = r.commit("c")
        findings, _ = di.check(d, None, head, lint=True)
        self.assertTrue(any("AGENTS.md" in f and "banned phrase" in f for f in findings), findings)

    def test_banned_phrase_obtain_re_review_fails(self):
        d, r = self._mk()
        r.write("CLAUDE.md", "Always obtain re-review before completion.\n")
        r.write("AGENTS.md", "(CLAUDE.md)\n")
        head = r.commit("c")
        findings, _ = di.check(d, None, head, lint=True)
        self.assertTrue(any("CLAUDE.md" in f and "banned phrase" in f for f in findings), findings)

    def test_near_match_obtains_re_review_passes(self):
        d, r = self._mk()
        r.write("AGENTS.md", "(CLAUDE.md)\n")
        r.write("docs/testing/evidence-contract.md", "The author fixes findings and obtains re-review.\n")
        head = r.commit("c")
        findings, _ = di.check(d, None, head, lint=True)
        self.assertEqual(findings, [])

    def test_near_match_plain_re_review_passes(self):
        d, r = self._mk()
        r.write("AGENTS.md", "(CLAUDE.md)\n")
        r.write("docs/orchestration/TASKS.md", "fix/re-review before scheduler integration.\n")
        head = r.commit("c")
        findings, _ = di.check(d, None, head, lint=True)
        self.assertEqual(findings, [])

    def test_missing_agents_md_fails(self):
        d, r = self._mk()
        r.write("CLAUDE.md", "ok\n")
        head = r.commit("c")
        findings, _ = di.check(d, None, head, lint=True)
        self.assertIn("lint: AGENTS.md missing", findings)

    def test_other_missing_scanned_file_is_skipped(self):
        d, r = self._mk()
        r.write("AGENTS.md", "(CLAUDE.md)\n")  # no CLAUDE.md, no TASKS.md, no docs/testing/
        head = r.commit("c")
        findings, _ = di.check(d, None, head, lint=True)
        self.assertEqual(findings, [])

    def test_missing_claude_link_fails(self):
        d, r = self._mk()
        r.write("AGENTS.md", "no link to the shared rules here\n")
        head = r.commit("c")
        findings, _ = di.check(d, None, head, lint=True)
        self.assertTrue(any("missing the (CLAUDE.md) link" in f for f in findings), findings)

    def test_clean_passes(self):
        d, r = self._mk()
        r.write("AGENTS.md", "(CLAUDE.md)\n")
        head = r.commit("c")
        findings, _ = di.check(d, None, head, lint=True)
        self.assertEqual(findings, [])


class Cli(unittest.TestCase):
    def test_without_base_prints_skip_and_runs_lint_only(self):
        d = tempfile.mkdtemp()
        r = Repo(d)
        r.write("AGENTS.md", "no link here\n")
        head = r.commit("c")
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            rc = di.main(["--repo", d, "--head", head])
        out = buf.getvalue()
        self.assertIn("guard skipped: no base", out)
        self.assertIn("lint: AGENTS.md missing the (CLAUDE.md) link", out)
        self.assertEqual(rc, 1)

    def test_no_lint_runs_guard_only(self):
        d = tempfile.mkdtemp()
        r = Repo(d)
        r.write("AGENTS.md", "no link here\n")  # would fail the lint if it ran
        r.write("big.txt", _lines("b", 250))
        base = r.commit("base")
        r.write("big.txt", "")
        head = r.commit("head")
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            rc = di.main(["--repo", d, "--base", base, "--head", head, "--no-lint"])
        out = buf.getvalue()
        self.assertIn("deletion: big.txt +0 -250", out)
        self.assertNotIn("lint:", out)
        self.assertEqual(rc, 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
