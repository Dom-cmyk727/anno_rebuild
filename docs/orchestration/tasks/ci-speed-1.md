# CI-SPEED-1: a time limit on every CI job

**Released 2026-11-27** by the orchestrator to the UI lane. The task runs on hosted CI only; it needs no local build
or test. Branch `task/CI-SPEED-1`, base origin/main. One concern, about 1 h.

Reviewed with EGM, stages 5-9 (reports in `E:\anno1503\cache\egm\ci-speed-1\`, local only). The review moved
compiler caching out of this packet. The repository is public, so runner minutes cost nothing. Caching only the four
Unix jobs would not shorten a run either, because `windows-msvc` alone sets a run's length (43.9 min median).
Caching all five jobs with one mechanism is the next packet, CI-SPEED-2 (draft:
[`ci-speed-2-draft.md`](ci-speed-2-draft.md)).

## Why

No job in `.github/workflows/ci.yml` has `timeout-minutes`. A hung job would run for GitHub's 6 hours, and
`tools/orch/land.py` would give up waiting for `gate` after its 120-min default without a result. No hung job has
been recorded; this is a precaution.

## Files

- `.github/workflows/ci.yml`: modify. Add job-level `timeout-minutes` only, one line per job key, next to `runs-on`.
- `docs/orchestration/REPORTS.md`: modify. Append the report.

## What to do

1. **Measure.** For each job key except `gate` (`changes`, `lint`, `test`, `build-macos`, `cpp`, `cpp-format`), find
   its longest job time.
   - Runs: `gh run list --workflow ci.yml --status completed --limit 60 --json databaseId`, then
     `gh run view <id> --json jobs` for each. Any branch and event counts.
   - Jobs: only those whose `conclusion` is `success` or `failure`. Skipped and cancelled jobs don't count.
   - Job time: `completedAt - startedAt`, in minutes. This is runner time, which is what `timeout-minutes`
     measures; queue time is not included.
   - Job key: the job name up to ` (`. For example, `cpp (windows-msvc)` belongs to `cpp`. A matrix job (`test`,
     `cpp`) takes its slowest entry, and its one limit applies to every entry.
   - If a key has fewer than 20 counted jobs, raise `--limit` until it has 20.
2. **Set the limits:** `timeout = min(90, ceil(1.5 × longest / 5) × 5)`, and at least 5.
   - **Why the cap is 90:** the chain `changes` → slowest job → `gate` must finish within `land.py`'s 120 min, queue
     waits included. With `changes` at about 5 and `gate` at 10, that leaves about 15 min for queueing.
   - **If the cap applies,** set 90 and flag it in the report; do not stop.
   - **`gate`** gets 10 min.
3. **Table in the report:** job key, longest job time, the run id it came from, and the value set.

## Do not touch

- `gate`'s name, `needs` list and classification logic. `land.py` requires the check `gate` on the exact landing
  SHA. A job that hits its limit ends as a failure, and `gate` then fails as it should.
- No `concurrency` / `cancel-in-progress`, and no other workflow change.

## Acceptance

- Hosted CI green on the exact head, and the diff adds only the `timeout-minutes` lines from the table.
- The table from step 3.
- The executor stops after the report. The orchestrator reviews the workflow diff and lands it with
  `land.py --allow-ci-change`.
- Report at the bottom of `docs/orchestration/REPORTS.md`, with "Decisions and reasoning". Use `/egm:egm-lite` and
  ponytail full. Start no reviewer subagents. That rule wins over anything egm-lite would start.
