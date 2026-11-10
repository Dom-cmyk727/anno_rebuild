"""Mirror every GitHub branch to the private GitLab remote and the co-developer's share (owner decisions
2026-11-06 and 2026-11-10).

GitHub (`origin`) stays the dispatch and CI channel. This copies `origin`'s branches to each mirror remote with
ordinary pushes only:

- `gitlab`: the private GitLab mirror (owner decision 2026-11-06);
- `share`: a bare repository in the Syncthing folder the co-developer reads (owner decision 2026-11-10; on the
  owner's PC `E:/open1503-share/to-codev/anno1503.git`). The co-developer fetches from his synced copy of it.

For every mirror:

- no force: a branch whose mirror head is not an ancestor of the GitHub head is reported and left alone;
- no deletes: a branch removed from GitHub stays on the mirror;
- nothing local: only what is already on GitHub is mirrored, so the GitHub branches are the one source.

A mirror that is not a remote of this clone is skipped with a message (the Macs have neither).

    python -I tools/orch/sync_gitlab.py [--repo E:/anno1503] [--mirror gitlab --mirror share] [--dry-run]

Run it from the owner's Windows PC (it holds the GitLab SSH key and the share) after every push to `main` and after
executors push their task branches. The exit status is 0 only when every configured mirror has every branch mirrored
or already equal.
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
    ap.add_argument("--repo", default=".", help="a clone that has the remotes (default: the current directory)")
    ap.add_argument("--source", default="origin")
    ap.add_argument("--mirror", action="append", help="a mirror remote; repeat for several (default: gitlab, share)")
    ap.add_argument("--dry-run", action="store_true", help="report what would be pushed, push nothing")
    a = ap.parse_args()

    git(a.repo, "fetch", "--quiet", "--prune", a.source)
    src = heads(a.repo, a.source)
    remotes = set(git(a.repo, "remote").stdout.split())
    rc = 0
    for mirror in a.mirror or ["gitlab", "share"]:
        if mirror not in remotes:
            print(f"== {mirror}: not a remote of this clone, skipped")
            continue
        print(f"== {mirror}")
        rc |= sync(a.repo, src, mirror, a.dry_run)
    return rc


def sync(repo: str, src: dict[str, str], mirror: str, dry_run: bool) -> int:
    """Pushes `src`'s branches to `mirror`; 0 when every branch is mirrored or already equal."""
    dst = heads(repo, mirror)

    push, same, diverged = [], [], []
    for name, sha in sorted(src.items()):
        old = dst.get(name)
        if old == sha:
            same.append(name)
            continue
        if old is not None:
            # The mirror's head must be known locally to test ancestry; fetch it without creating a ref.
            git(repo, "fetch", "--quiet", mirror, old, check=False)
            if git(repo, "merge-base", "--is-ancestor", old, sha, check=False).returncode != 0:
                diverged.append((name, old, sha))
                continue
        push.append((name, old, sha))

    for name, old, sha in push:
        print(f"{'would push' if dry_run else 'push'} {name}: {old[:8] if old else 'new'} -> {sha[:8]}")
    for name, old, sha in diverged:
        print(f"SKIP {name}: {mirror} {old[:8]} is not an ancestor of GitHub {sha[:8]} (not forced; resolve by hand)")
    print(f"{len(same)} equal, {len(push)} to push, {len(diverged)} diverged")

    rc = 0
    if push and not dry_run:
        specs = [f"{sha}:refs/heads/{name}" for name, _, sha in push]
        r = subprocess.run(["git", "-C", repo, "push", "--porcelain", mirror, *specs], capture_output=True, text=True)
        for line in r.stdout.splitlines():
            if line.startswith("!"):
                print("REJECTED", line)
        if r.returncode != 0:
            print(r.stderr.strip(), file=sys.stderr)
            rc = 1
        after = heads(repo, mirror)
        missing = [name for name, _, sha in push if after.get(name) != sha]
        if missing:
            print("not mirrored:", ", ".join(missing))
            rc = 1
    return 1 if diverged else rc


if __name__ == "__main__":
    sys.exit(main())
