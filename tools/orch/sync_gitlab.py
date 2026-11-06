"""Mirror every GitHub branch to the private GitLab remote (owner decision 2026-11-06).

The co-developer works from the private GitLab mirror; GitHub (`origin`) stays the dispatch and CI channel. This
copies `origin`'s branches to the `gitlab` remote with ordinary pushes only:

- no force: a branch whose GitLab head is not an ancestor of the GitHub head is reported and left alone;
- no deletes: a branch removed from GitHub stays on GitLab;
- nothing local: only what is already on GitHub is mirrored, so the GitHub branches are the one source.

    python -I tools/orch/sync_gitlab.py [--repo E:/anno1503] [--dry-run]

Run it from a host with the GitLab SSH key (the owner's Windows PC) after every push to `main` and after executors
push their task branches. The exit status is 0 only when every branch is mirrored or already equal.
"""
from __future__ import annotations

import argparse
import subprocess
import sys


def git(repo: str, *args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(["git", "-C", repo, *args], capture_output=True, text=True, check=check)


def heads(repo: str, remote: str) -> dict[str, str]:
    out = git(repo, "ls-remote", "--heads", remote).stdout
    result = {}
    for line in out.splitlines():
        sha, ref = line.split("\t", 1)
        result[ref.removeprefix("refs/heads/")] = sha
    return result


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--repo", default=".", help="a clone that has both remotes (default: the current directory)")
    ap.add_argument("--source", default="origin")
    ap.add_argument("--mirror", default="gitlab")
    ap.add_argument("--dry-run", action="store_true", help="report what would be pushed, push nothing")
    a = ap.parse_args()

    git(a.repo, "fetch", "--quiet", "--prune", a.source)
    src = heads(a.repo, a.source)
    dst = heads(a.repo, a.mirror)

    push, same, diverged = [], [], []
    for name, sha in sorted(src.items()):
        old = dst.get(name)
        if old == sha:
            same.append(name)
            continue
        if old is not None:
            # The mirror's head must be known locally to test ancestry; fetch it without creating a ref.
            git(a.repo, "fetch", "--quiet", a.mirror, old, check=False)
            if git(a.repo, "merge-base", "--is-ancestor", old, sha, check=False).returncode != 0:
                diverged.append((name, old, sha))
                continue
        push.append((name, old, sha))

    for name, old, sha in push:
        print(f"{'would push' if a.dry_run else 'push'} {name}: {old[:8] if old else 'new'} -> {sha[:8]}")
    for name, old, sha in diverged:
        print(f"SKIP {name}: GitLab {old[:8]} is not an ancestor of GitHub {sha[:8]} (not forced; resolve by hand)")
    print(f"{len(same)} equal, {len(push)} to push, {len(diverged)} diverged")

    rc = 0
    if push and not a.dry_run:
        specs = [f"{sha}:refs/heads/{name}" for name, _, sha in push]
        r = subprocess.run(["git", "-C", a.repo, "push", "--porcelain", a.mirror, *specs], capture_output=True, text=True)
        for line in r.stdout.splitlines():
            if line.startswith("!"):
                print("REJECTED", line)
        if r.returncode != 0:
            print(r.stderr.strip(), file=sys.stderr)
            rc = 1
        after = heads(a.repo, a.mirror)
        missing = [name for name, _, sha in push if after.get(name) != sha]
        if missing:
            print("not mirrored:", ", ".join(missing))
            rc = 1
    return 1 if diverged else rc


if __name__ == "__main__":
    sys.exit(main())
