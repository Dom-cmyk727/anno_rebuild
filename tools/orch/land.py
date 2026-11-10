"""Land one commit on main through the required CI check (owner decision 2026-11-10).

`main` on GitHub is protected: GitHub accepts a push to it only for a commit on which the `gate` job of
`.github/workflows/ci.yml` has already passed, and no force push or deletion. The rule also binds admins, so it binds
every orchestrator (they all push as the owner's account). `gate` passes when every CI job passed, or, for a change
that touches only `docs/` and root `*.md` files, when the build jobs were skipped. CI runs on a push to any branch,
so a commit gets its check on a landing branch first:

    python -I tools/orch/land.py [--repo E:/anno1503] [--rev main] [--name <topic>] [--timeout-min 120] [--no-sync]

1. fetch `origin`; the commit must be a descendant of `origin/main` (fast-forward only: merge or rebase first);
2. unless `gate` already passed on that commit: push it to `land/<name>` and wait for `gate` on that exact
   commit (about 30 minutes for a code change, about 1 for a docs-only one);
3. push the commit to `main`, delete `land/<name>`, then mirror to GitLab and the co-developer's share
   (`tools/orch/sync_gitlab.py`) unless `--no-sync`.

A failed `gate` stops with its link and leaves `land/<name>` in place. Re-run the failed jobs
(`gh run rerun <run id> --failed`, which re-runs `gate` too), or fix and commit again, then run this again.
Exit status: 0 landed or already on main; 1 refused before pushing; 2 gate failed or timed out; 3 main refused.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

CHECK = "gate"


def run(*cmd: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(list(cmd), capture_output=True, text=True, check=check)


def gate_state(check_runs: list[dict]) -> tuple[str, dict | None]:
    """The state of the newest `gate` check run: absent, pending, success or failure.

    Only the newest run counts (a re-run supersedes an older verdict, as on GitHub), and only the conclusion
    "success" passes: "skipped" or "neutral" would satisfy a GitHub requirement, so they must not satisfy this one.
    """
    runs = [r for r in check_runs if r.get("name") == CHECK]
    if not runs:
        return "absent", None
    newest = max(runs, key=lambda r: r["id"])
    if newest.get("status") != "completed":
        return "pending", newest
    return ("success" if newest.get("conclusion") == "success" else "failure"), newest


def gate_of(slug: str, sha: str) -> tuple[str, dict | None]:
    r = run("gh", "api", f"repos/{slug}/commits/{sha}/check-runs?check_name={CHECK}&per_page=100", check=False)
    if r.returncode != 0:
        if "No commit found for SHA" in r.stdout + r.stderr:  # HTTP 422: GitHub has not seen the commit yet
            return "absent", None
        raise RuntimeError(f"gh api check-runs failed for {sha[:8]}:\n{r.stdout}{r.stderr}")
    return gate_state(json.loads(r.stdout)["check_runs"])


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--repo", default=".", help="the clone to land from (default: the current directory)")
    ap.add_argument("--rev", default="HEAD", help="the commit to land (default: HEAD)")
    ap.add_argument("--name", help="landing branch name after land/ (default: land-<commit>)")
    ap.add_argument("--timeout-min", type=int, default=120, help="give up waiting for gate after this long")
    ap.add_argument("--no-sync", action="store_true", help="do not mirror to GitLab and the share afterwards")
    a = ap.parse_args()

    def git(*args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
        return run("git", "-C", a.repo, *args, check=check)

    git("fetch", "--quiet", "origin")
    sha = git("rev-parse", "--verify", f"{a.rev}^{{commit}}").stdout.strip()
    main_sha = git("rev-parse", "origin/main").stdout.strip()
    if sha == main_sha:
        print(f"{sha[:8]} is origin/main already: nothing to land")
        return 0
    if git("merge-base", "--is-ancestor", main_sha, sha, check=False).returncode != 0:
        print(f"REFUSED: {sha[:8]} does not descend from origin/main {main_sha[:8]}; merge or rebase onto it first")
        return 1
    slug = run("gh", "repo", "view", git("remote", "get-url", "origin").stdout.strip(),
               "--json", "nameWithOwner", "-q", ".nameWithOwner").stdout.strip()
    branch = f"land/{a.name or 'land-' + sha[:8]}"
    ahead = git("rev-list", "--count", f"{main_sha}..{sha}").stdout.strip()
    print(f"landing {sha[:8]} on {slug} main ({ahead} commit(s) ahead of {main_sha[:8]})")

    state, check_run = gate_of(slug, sha)
    if state == "absent":
        r = git("push", "--porcelain", "origin", f"{sha}:refs/heads/{branch}", check=False)
        if r.returncode != 0:
            print(f"REFUSED: push to {branch} failed:\n{r.stdout}{r.stderr}")
            return 1
        print(f"pushed to {branch}; waiting for {CHECK}")
    deadline = time.monotonic() + 60 * a.timeout_min
    last = None
    while state in ("absent", "pending"):
        if time.monotonic() > deadline:
            print(f"TIMEOUT: {CHECK} still {state} after {a.timeout_min} min on {sha[:8]}; {branch} left in place")
            return 2
        if state != last:
            print(f"  {CHECK}: {state}" + (f" ({check_run['html_url']})" if check_run else ""), flush=True)
            last = state
        time.sleep(30)
        state, check_run = gate_of(slug, sha)
    if state != "success":
        print(f"GATE FAILED on {sha[:8]}: {check_run.get('conclusion')} {check_run['html_url']}")
        print(f"  {branch} left in place; re-run the failed jobs (gh run rerun <run id> --failed) or fix, then rerun")
        return 2
    print(f"  {CHECK}: success ({check_run['html_url']})")

    r = git("push", "--porcelain", "origin", f"{sha}:refs/heads/main", check=False)
    if r.returncode != 0:
        print(f"MAIN REFUSED the push of {sha[:8]}:\n{r.stdout}{r.stderr}")
        return 3
    print(f"main: {main_sha[:8]} -> {sha[:8]}")
    remote = git("ls-remote", "origin", f"refs/heads/{branch}").stdout.split()
    if remote and remote[0] == sha:
        git("push", "--quiet", "origin", "--delete", branch, check=False)
        print(f"deleted {branch}")
    if not a.no_sync:
        s = run(sys.executable, "-I", str(Path(__file__).with_name("sync_gitlab.py")), "--repo", a.repo, check=False)
        print(s.stdout.strip())
        if s.returncode != 0:
            print(f"mirror sync reported a problem (exit {s.returncode}); main is landed regardless\n{s.stderr}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
