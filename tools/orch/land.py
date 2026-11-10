"""Land one commit on main through the required CI check (owner decision 2026-11-10).

`main` on GitHub is protected: GitHub accepts a push to it only for a commit on which the `gate` job of
`.github/workflows/ci.yml` has already passed, and no force push or deletion. The rule also binds admins, so it binds
every orchestrator (they all push as the owner's account). `gate` passes when every CI job passed, or, for a change
that touches only `docs/` and root `*.md` files, when the build jobs were skipped. CI runs on a push to any branch,
so a commit gets its check on a landing branch first:

    python -I tools/orch/land.py [--repo E:/anno1503] [--rev HEAD] [--name <topic>] [--allow-ci-change]
                                 [--timeout-min 120] [--no-sync]

1. fetch `origin`; the commit must descend from `origin/main` (fast-forward only: merge or rebase first). A commit
   that changes `.github/` is refused unless `--allow-ci-change` says its workflow changes were reviewed: the commit
   being checked defines `gate`, so a weakened workflow would approve itself;
2. unless `gate` already passed on that commit: push it to `land/<name>` and wait for `gate` on that exact commit
   (about 30 minutes for a code change, about 1 for a docs-only one). Only a `gate` of `ci.yml`, run for a push of
   this very commit, counts: not one from a pull request (its base may be another branch), from another workflow,
   or for another commit;
3. push the commit to `main`, delete `land/<name>`, then mirror to GitLab and the co-developer's share
   (`tools/orch/sync_gitlab.py`) unless `--no-sync`.

A failed `gate` stops with its link and leaves `land/<name>` in place. Re-run the failed jobs
(`gh run rerun <run id> --failed`, which re-runs `gate` too) and run this again: it waits for the re-run. Or fix,
commit and land the new commit.
Exit status: 0 landed or already on main; 1 refused before pushing anything; 2 gate failed, no CI run started,
timed out or GitHub kept failing (the landing branch stays); 3 main refused the push.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

CHECK = "gate"
WORKFLOW = ".github/workflows/ci.yml"
MAIN = "refs/remotes/origin/main"  # the full ref: a tag named "origin/main" would win over the short name
POLL_S = 30
NO_RUN_MIN = 10  # no CI run for the commit this long after the push: it will not come ([skip ci], a broken ci.yml)
MAX_API_ERRORS = 5


def run(*cmd: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    # git and gh write UTF-8; Windows' default code page would garble a non-ASCII path.
    return subprocess.run(list(cmd), capture_output=True, encoding="utf-8", errors="replace", check=check)


def counts(check_run: dict, sha: str) -> bool:
    """A `gate` check run counts only when ci.yml ran it for a push of exactly this commit."""
    wf = check_run.get("workflow") or {}
    return (check_run.get("name") == CHECK and wf.get("path") == WORKFLOW and wf.get("event") == "push"
            and wf.get("head_sha") == sha)


def gate_state(check_runs: list[dict], sha: str) -> tuple[str, dict | None]:
    """The state of the newest counting `gate` check run: absent, pending, success or failure.

    Each check run carries "workflow": the workflow run it belongs to (path, event, head_sha, status). Only the newest
    counting run decides (a re-run supersedes an older verdict, as on GitHub), and only the conclusion "success"
    passes: GitHub would accept "skipped" or "neutral" for a required check, this does not. A failed verdict whose
    workflow run is running again (`gh run rerun --failed`) is pending: the re-run's own `gate` appears only when its
    jobs are done.
    """
    runs = [r for r in check_runs if counts(r, sha)]
    if not runs:
        return "absent", None
    newest = max(runs, key=lambda r: r["id"])
    if newest.get("status") != "completed":
        return "pending", newest
    if newest.get("conclusion") == "success":
        return "success", newest
    if newest["workflow"].get("status") != "completed":
        return "pending", newest
    return "failure", newest


def api(path: str) -> dict:
    r = run("gh", "api", path, check=False)
    if r.returncode != 0:
        if "No commit found for SHA" in r.stdout + r.stderr:  # HTTP 422: GitHub has not seen the commit yet
            return {}
        raise RuntimeError(f"gh api {path} failed:\n{r.stdout}{r.stderr}")
    return json.loads(r.stdout)


def gate_of(slug: str, sha: str) -> tuple[str, dict | None]:
    check_runs = api(f"repos/{slug}/commits/{sha}/check-runs?check_name={CHECK}&per_page=100").get("check_runs", [])
    suites: dict[int, dict | None] = {}
    for cr in check_runs:
        suite = (cr.get("check_suite") or {}).get("id")
        if suite is not None and suite not in suites:
            wf = api(f"repos/{slug}/actions/runs?check_suite_id={suite}").get("workflow_runs") or [None]
            suites[suite] = wf[0]
        cr["workflow"] = suites.get(suite)
    return gate_state(check_runs, sha)


def changed_paths(repo: str, base: str, sha: str) -> list[str]:
    """Every path changed from `base` to `sha`, unquoted (-z: git would quote a non-ASCII path otherwise)."""
    out = run("git", "-C", repo, "diff", "--name-only", "--no-renames", "-z", base, sha).stdout
    return [p for p in out.split("\0") if p]


def ci_runs_exist(slug: str, sha: str) -> bool:
    runs = api(f"repos/{slug}/actions/runs?head_sha={sha}&event=push&per_page=100").get("workflow_runs", [])
    return any(r.get("path") == WORKFLOW for r in runs)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--repo", default=".", help="the clone to land from (default: the current directory)")
    ap.add_argument("--rev", default="HEAD", help="the commit to land (default: HEAD)")
    ap.add_argument("--name", help="landing branch name after land/ (default: land-<commit>)")
    ap.add_argument("--allow-ci-change", action="store_true",
                    help="the commit changes .github/ and those changes were reviewed")
    ap.add_argument("--timeout-min", type=int, default=120, help="give up waiting for gate after this long")
    ap.add_argument("--no-sync", action="store_true", help="do not mirror to GitLab and the share afterwards")
    a = ap.parse_args()

    def git(*args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
        return run("git", "-C", a.repo, *args, check=check)

    git("fetch", "--quiet", "origin")
    sha = git("rev-parse", "--verify", f"{a.rev}^{{commit}}").stdout.strip()
    main_sha = git("rev-parse", "--verify", f"{MAIN}^{{commit}}").stdout.strip()
    if sha == main_sha:
        print(f"{sha[:8]} is origin/main already: nothing to land")
        return 0
    if git("merge-base", "--is-ancestor", main_sha, sha, check=False).returncode != 0:
        print(f"REFUSED: {sha[:8]} does not descend from origin/main {main_sha[:8]}; merge or rebase onto it first")
        return 1
    ci_files = [f for f in changed_paths(a.repo, main_sha, sha) if f.startswith(".github/")]
    if ci_files and not a.allow_ci_change:
        print(f"REFUSED: {sha[:8]} changes CI ({', '.join(ci_files)}). The commit defines its own gate: review those "
              "changes, then land with --allow-ci-change")
        return 1
    slug = run("gh", "repo", "view", git("remote", "get-url", "origin").stdout.strip(),
               "--json", "nameWithOwner", "-q", ".nameWithOwner").stdout.strip()
    branch = f"land/{a.name or 'land-' + sha[:8]}"
    ahead = git("rev-list", "--count", f"{main_sha}..{sha}").stdout.strip()
    print(f"landing {sha[:8]} on {slug} main ({ahead} commit(s) ahead of {main_sha[:8]})"
          + (f"; CI changes allowed: {', '.join(ci_files)}" if ci_files else ""))

    try:
        state, check_run = gate_of(slug, sha)
    except (RuntimeError, ValueError, KeyError) as e:
        print(f"REFUSED: cannot read the gate of {sha[:8]}: {e}")
        return 1
    pushed_at = None
    if state == "absent":
        r = git("push", "--porcelain", "origin", f"{sha}:refs/heads/{branch}", check=False)
        if r.returncode != 0:
            print(f"REFUSED: push to {branch} failed:\n{r.stdout}{r.stderr}")
            return 1
        pushed_at = time.monotonic()
        print(f"pushed to {branch}; waiting for {CHECK}")
    left = f"{branch} left in place" if pushed_at is not None else "nothing was pushed"
    deadline = time.monotonic() + 60 * a.timeout_min
    last, errors = None, 0
    while state in ("absent", "pending"):
        if time.monotonic() > deadline:
            print(f"TIMEOUT: {CHECK} still {state} after {a.timeout_min} min on {sha[:8]}; {left}")
            return 2
        if state != last:
            print(f"  {CHECK}: {state}" + (f" ({check_run['html_url']})" if check_run else ""), flush=True)
            last = state
        time.sleep(POLL_S)
        try:
            state, check_run = gate_of(slug, sha)
            if (state == "absent" and pushed_at is not None and time.monotonic() - pushed_at > 60 * NO_RUN_MIN
                    and not ci_runs_exist(slug, sha)):
                print(f"NO CI RUN for {sha[:8]} {NO_RUN_MIN} min after the push ([skip ci] in the message, or a broken "
                      f"{WORKFLOW}?); {left}")
                return 2
            errors = 0
        except (RuntimeError, ValueError, KeyError) as e:
            errors += 1
            print(f"  GitHub error {errors}/{MAX_API_ERRORS}: {e}", flush=True)
            if errors >= MAX_API_ERRORS:
                print(f"STOPPED after {errors} GitHub errors in a row; {left}")
                return 2
    if state != "success":
        print(f"GATE FAILED on {sha[:8]}: {check_run.get('conclusion')} {check_run['html_url']}")
        print(f"  {left}; re-run the failed jobs (gh run rerun <run id> --failed) and run this again, or fix")
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
