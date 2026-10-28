"""One-call start checks for an executor task (orchestration tooling, round ct).

The orchestrator puts the exact command into the task packet; the executor runs
it once instead of separate git calls. Every check prints one PASS/FAIL row and
the exit status is 0 only when all of them pass.

    python -I tools/orch/preflight.py --main E:/anno1503 --main-at <commit> \
        [--untracked AGENTS.md] [--board-at <commit>] \
        [--worktree <path> --head <commit> [--branch <name>] [--clean]] ...

"--main-at" accepts the given commit or any descendant (the board may have moved
on). "--board-at" is the board commit the packet was generated from; the check
fails when docs/orchestration/TASKS.md on main changed since then, so the packet's
excerpts would be stale. A --worktree group ends at the next --worktree.
"""
from __future__ import annotations

import argparse
import subprocess
import sys

BOARD = "docs/orchestration/TASKS.md"


def git(repo: str, *args: str) -> tuple[int, str]:
    p = subprocess.run(["git", "-C", repo, *args], capture_output=True, text=True)
    return p.returncode, (p.stdout if p.returncode == 0 else p.stderr).strip()


def full(repo: str, rev: str) -> str | None:
    code, out = git(repo, "rev-parse", "--verify", rev + "^{commit}")
    return out if code == 0 else None


class Report:
    def __init__(self) -> None:
        self.rows: list[tuple[bool, str, str]] = []

    def check(self, ok: bool, what: str, detail: str) -> None:
        self.rows.append((ok, what, detail))

    def print(self) -> int:
        width = max(len(w) for _, w, _ in self.rows)
        for ok, what, detail in self.rows:
            print("%s  %-*s  %s" % ("PASS" if ok else "FAIL", width, what, detail))
        bad = sum(1 for ok, _, _ in self.rows if not ok)
        print("preflight: %s (%d checks, %d failed)" % ("OK" if bad == 0 else "FAILED", len(self.rows), bad))
        return 0 if bad == 0 else 1


def status_lines(repo: str) -> list[str] | None:
    code, out = git(repo, "status", "--short", "--untracked-files=all")
    if code != 0:
        return None
    return [line for line in out.splitlines() if line.strip()]


def check_main(r: Report, main: str, at: str, untracked: list[str], board_at: str | None) -> None:
    head = full(main, "HEAD")
    want = full(main, at)
    if head is None or want is None:
        r.check(False, "main HEAD", "cannot resolve HEAD or %s in %s" % (at, main))
        return
    code, _ = git(main, "merge-base", "--is-ancestor", want, head)
    r.check(code == 0, "main HEAD at %s or later" % at[:12], "HEAD %s" % head)
    lines = status_lines(main)
    allowed = {"?? " + u.replace("\\", "/") for u in untracked}
    extra = [line for line in (lines or []) if line not in allowed]
    r.check(lines is not None and not extra, "main status only %s" % (", ".join(untracked) or "clean"),
            "unexpected: " + "; ".join(extra) if extra else "ok")
    if board_at:
        b = full(main, board_at)
        if b is None:
            r.check(False, "board unchanged since packet", "cannot resolve %s" % board_at)
        else:
            code, _ = git(main, "diff", "--quiet", b, head, "--", BOARD)
            r.check(code == 0, "board unchanged since packet %s" % board_at[:12],
                    "ok" if code == 0 else "TASKS.md changed after the packet: re-read the changed blocks on main")


def check_worktree(r: Report, path: str, head: str, branch: str | None, clean: bool) -> None:
    actual = full(path, "HEAD")
    want = full(path, head)
    r.check(actual is not None and actual == want, "%s HEAD %s" % (path, head[:12]),
            "HEAD %s" % (actual or "unresolvable"))
    if branch:
        code, out = git(path, "rev-parse", "--abbrev-ref", "HEAD")
        r.check(code == 0 and out == branch, "%s branch %s" % (path, branch), "on %s" % out)
    if clean:
        lines = status_lines(path)
        r.check(lines == [], "%s clean" % path, "ok" if lines == [] else "; ".join(lines or ["status failed"]))


def main(argv: list[str]) -> int:
    # Split the argument list into the main part and one group per --worktree.
    groups: list[list[str]] = [[]]
    for a in argv:
        if a == "--worktree":
            groups.append([])
        groups[-1].append(a)
    head_parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    head_parser.add_argument("--main", required=True)
    head_parser.add_argument("--main-at", required=True)
    head_parser.add_argument("--untracked", action="append", default=[])
    head_parser.add_argument("--board-at")
    tree_parser = argparse.ArgumentParser(prog="--worktree group")
    tree_parser.add_argument("--worktree", required=True)
    tree_parser.add_argument("--head", required=True)
    tree_parser.add_argument("--branch")
    tree_parser.add_argument("--clean", action="store_true")
    top = head_parser.parse_args(groups[0])
    r = Report()
    check_main(r, top.main, top.main_at, top.untracked, top.board_at)
    for g in groups[1:]:
        t = tree_parser.parse_args(g)
        check_worktree(r, t.worktree, t.head, t.branch, t.clean)
    return r.print()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
