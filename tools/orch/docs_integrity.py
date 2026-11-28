"""Two landing guards, run against a git range (HARNESS-GUARD-1).

The deletion guard flags a large, unexplained cut (e.g. a doc trimmed by mistake); the rule-sync
lint flags project docs that still carry wording harness item 1 removed. Both run in CI on every
push (`.github/workflows/ci.yml`, job `docs-integrity`) and in `land.py` before a push to `main`.

    python -I tools/orch/docs_integrity.py [--base <rev>] --head <rev> [--no-lint] [--repo .]

Without `--base`, only the lint runs, and the script prints "guard skipped: no base" (the deletion
guard needs a base to diff against; `land.py` always has one). `--no-lint` runs only the guard (used
by the history replay below, since older commits still carry the banned wording).

Exit codes: 0 clean, 1 at least one finding, 2 a git error.

Output, one line each:
  deletion: <path> +<added> -<deleted>
  lint: <path> <rule>
  trailer: <path> - <reason>      (for each honoured Large-Deletion trailer)
"""
from __future__ import annotations

import argparse
import subprocess
import sys

BANNED_PHRASES = ("use a review subagent", "obtain re-review before completion")
DELETION_FLOOR = 200
DELETION_RATIO = 4


def _git(repo: str, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(["git", "-C", repo, *args], capture_output=True, encoding="utf-8",
                          errors="replace", check=True)


def _numstat(repo: str, base: str, head: str) -> list[tuple[str, str, str]]:
    """(added, deleted, path) per changed file; `added`/`deleted` are "-" for a binary file."""
    out = _git(repo, "diff", "--numstat", "-z", "--no-renames", base, head).stdout
    rows = []
    for part in out.split("\0"):
        if not part:
            continue
        added, deleted, path = part.split("\t", 2)
        rows.append((added, deleted, path))
    return rows


def _trailers(repo: str, base: str, head: str) -> list[str]:
    """Every `Large-Deletion` trailer value in `base..head` (git drops commits without one as a
    blank line; we filter those out here)."""
    out = _git(repo, "log", "--format=%(trailers:key=Large-Deletion,valueonly,unfold)",
              f"{base}..{head}").stdout
    return [line for line in out.split("\n") if line.strip()]


def _honours(path: str, trailers: list[str]) -> str | None:
    """The reason text of the first trailer that names `path`, or None."""
    prefix = path + " - "
    for t in trailers:
        if t.startswith(prefix) and t[len(prefix):].strip():
            return t[len(prefix):]
    return None


def _deletion_guard(repo: str, base: str, head: str) -> tuple[list[str], list[str]]:
    flagged: dict[str, tuple[int, int]] = {}
    for added_s, deleted_s, path in _numstat(repo, base, head):
        if added_s == "-" or deleted_s == "-":  # binary file: no line counts
            continue
        added, deleted = int(added_s), int(deleted_s)
        if deleted - added >= DELETION_FLOOR and deleted >= DELETION_RATIO * added:
            flagged[path] = (added, deleted)
    trailers = _trailers(repo, base, head)
    findings, honoured = [], []
    for path in sorted(flagged):
        added, deleted = flagged[path]
        reason = _honours(path, trailers)
        if reason is not None:
            honoured.append(f"trailer: {path} - {reason}")
        else:
            findings.append(f"deletion: {path} +{added} -{deleted}")
    return findings, honoured


def _scanned_files(repo: str, head: str) -> list[str]:
    files = ["AGENTS.md", "CLAUDE.md", "docs/orchestration/TASKS.md"]
    testing = _git(repo, "ls-tree", "--name-only", head, "docs/testing/").stdout
    files += [name for name in testing.split("\n") if name.strip()]
    return files


def _show(repo: str, head: str, path: str) -> str | None:
    """The file's content at `head`, or None when it does not exist there."""
    r = subprocess.run(["git", "-C", repo, "show", f"{head}:{path}"], capture_output=True, check=False)
    if r.returncode != 0:
        return None
    return r.stdout.decode("utf-8", errors="replace")


def _lint(repo: str, head: str) -> list[str]:
    findings = []
    for path in _scanned_files(repo, head):
        content = _show(repo, head, path)
        if content is None:
            if path == "AGENTS.md":  # every other scanned file may be absent; AGENTS.md may not
                findings.append(f"lint: {path} missing")
            continue
        lower = content.lower()
        for phrase in BANNED_PHRASES:
            if phrase in lower:
                findings.append(f"lint: {path} banned phrase {phrase!r}")
        if path == "AGENTS.md" and "(CLAUDE.md)" not in content:
            findings.append(f"lint: {path} missing the (CLAUDE.md) link")
    return findings


def check(repo: str, base: str | None, head: str, lint: bool = True) -> tuple[list[str], list[str]]:
    """Every `deletion:`/`lint:` finding and every honoured `trailer:` line. Raises
    subprocess.CalledProcessError on a git error."""
    findings: list[str] = []
    honoured: list[str] = []
    if base:
        d_findings, honoured = _deletion_guard(repo, base, head)
        findings += d_findings
    if lint:
        findings += _lint(repo, head)
    return findings, honoured


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--repo", default=".")
    ap.add_argument("--base")
    ap.add_argument("--head", required=True)
    ap.add_argument("--no-lint", action="store_true")
    a = ap.parse_args(argv)
    if not a.base:
        print("guard skipped: no base")
    try:
        findings, honoured = check(a.repo, a.base, a.head, lint=not a.no_lint)
    except subprocess.CalledProcessError as e:
        print(f"git error: {e}\n{e.stderr}")
        return 2
    for line in honoured + findings:
        print(line)
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
