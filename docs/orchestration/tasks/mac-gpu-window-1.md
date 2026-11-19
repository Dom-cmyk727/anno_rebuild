# MAC-GPU-WINDOW-1 — candidate 10 first-frame diagnosis

**RELEASED to EXECUTOR II MAC, 2026-11-19.** Owner-forwarded Windows request.
One small read-only measurement packet; no code or harness change. This takes
priority over the next wildlife gameplay packet at the completed KERNEL checkpoint.
Preserve the clean kernel branch, all retained evidence and ongoing hosted CI.

## 0. Pin, context and reading

Use the existing detached candidate-10 gate checkout/build, not the newer wildlife
binary. Candidate head `95897a55bc939a185766b7207e69d098997cdb1d`, tree
`d65099256a598f7ce82d37ea303b81ff87088542`. Verify both, clean tracked status,
CMake source/build identity and executable hash. Reuse its existing Release build
with viewer/GPU enabled. Do not rebuild a different revision, reset/merge/rebase,
change tracked files, delete evidence, rerun the whole suite or change timeouts.
A missing/stale candidate binary is a precise intake blocker, not permission to
substitute a new build. The original full run remains FAIL.

Read CLAUDE, evidence-contract/testing README, the exact candidate's registration
in `src/app/CMakeLists.txt`, `tools/test/viewer_wiring.py`,
`docs/app/window-trace.md`, relevant `Viewer::script_tick`, main-loop draw gate,
SDL event handling and acquire/back-off logic in `src/app/src/viewer.cpp`.
Read the preserved gate handoff and raw outputs under
`cache/gate-run-ui-info-2d-buildcat/`, especially outcomes-artifacts/1084 and 1085,
plus `docs/orchestration/reviews/2026-11-17-gate-ui-info-2d-buildcat-review.md`
from the orchestrator branch (read without merging).

**Owner-confirmed original condition: the display was UNLOCKED AND AWAKE.**
The earlier locked/asleep/SSH explanation is an unverified hypothesis, not the
recorded condition. Do not relabel a locked-screen experiment as a reproduction.
Window visibility/coverage/focus during the original run remain to be established.

The UI lane's static lead: no frame completed before the first script line. Two
potential draw blockers are `hidden_` after SDL hidden/minimized/occluded events,
and `retry_at_` after unsuccessful surface acquire. Metal can withhold a drawable
for an invisible layer. This is a hypothesis to distinguish, not a conclusion.

## 1. Prepare four bounded runs

Exact cases:

- `viewer_wiring_ui_magnifier_range`
- `viewer_wiring_ui_magnifier_range_stepped`

Run each ALONE once in each condition, four total. Use the candidate's registered
commands/arguments, real assets and GPU/Metal path, one job, no other owned GPU
work. Admission stays on. Record current CPU/RAM/disk/load, display/session state,
frontmost app, window visibility/coverage, drawable size, adapter and source/binary
identities. Do not disturb another executor or coordinator. Preserve the owner's
awake assertion; do not silently lock/sleep the Mac, change display policy or
inject input into a test session.

A — explicitly controlled visible session: unlocked and awake, terminal
frontmost as requested. Arrange its footprint so it does not completely cover
the spawned game window; verify actual viewer visibility separately from keyboard
focus. A terminal being frontmost alone does not prove the game layer is visible.
Use supported UI controls or the owner's assistance if needed; record what was
actually achieved, not just the requested setup.

B — original-launch context: reproduce the known original context from the
preserved gate's command/environment and executor launch method, with display
still unlocked and awake. Leave the normal automatically launched window/focus
behavior intact and observe it. Recover any evidence of the old window arrangement
from logs; never assert an exact reconstruction if coverage/focus was not recorded.
Run only these two cases serially, not the old full gate or manufactured CPU/GPU
load. Explicitly list residual differences from the original four-job campaign.
If A and B turn out to have the same observable state, say so; this is not a
controlled visible-versus-invisible comparison. Missing historical context stays
UNKNOWN. Do not add an unrequested locked-screen or remote-session campaign.

## 2. Retain evidence before cleanup

Use fresh output directories `cache/mac-gpu-window-1/A/<case>` and `B/<case>`;
never overwrite old artifacts. Preserve wrapper stdout/stderr, child stdout/stderr,
script log, exit status, elapsed time, CTest/gate inventory and every existing
trace/artifact before temporary cleanup. Validate retained paths and nonempty
files. Hash the artifacts. Record exact commands and environment differences.

Prefer the canonical runner with an exact anchored selector for each case and
existing trace environment if supported. Existing `ANNO_VIEW_TRACE` or wrapper
`--trace` may be used as diagnostic instrumentation without editing source,
registration or assertions. Keep instrumentation identical in A and B, and label
it as an instrumentation difference from the original run. If a direct original
wrapper command is necessary for output retention, label it an investigation,
not a canonical acceptance gate. Do not count required contracts as extra window
runs. An unavailable display is a missing prerequisite, not a passed magnifier.

Look for retained acquire statuses/timeouts, `acquire_skipped`/retry/back-off
records, and available SDL occluded/hidden/minimized/exposed evidence. The existing
trace can show acquire results; check whether this exact revision records SDL
visibility events at all. Do not invent an absent event log. Silence alone cannot
prove either `hidden_` or `retry_at_`. No added logging, test seam or patch here.

## 3. Attribute only what the observations establish

Produce a four-row result table: condition, case, actual display/window state,
pass/fail, first-frame progress, first-script progress, acquire/visibility evidence,
blocking-path verdict and stderr/trace receipt.

Classify the result as one of:

- `hidden_` established by direct state/event evidence;
- acquire/back-off established by acquire statuses and control-flow evidence;
- both paths observed, with sequence;
- neither reproduced / insufficient observation to distinguish.

A 30-second no-frame message alone establishes neither path. A pass after arranging
visibility supports a condition difference, not by itself the original failure's
cause. A passing script establishes this run's magnifier assertions, not physical
scanout or all Metal rendering. If all four pass, report 4/4 with the original
failure unresolved; do not loop until something fails or erase the old result.
If existing logging cannot identify the path, report the exact observation gap
for Windows' proposed harness follow-up. That is valid bounded closeout.

## 4. Revised Mac skip policy — record, do not rerun

The owner forwards Windows' reconciliation of candidate 10's skip inventory:
TEN named-adapter cases are expected `gpu` skips on this Mac: the two play cases
and eight RTX/Radeon atlas/RENDER-DIRECT cases. The exact names/reasons are in
`cache/gate-run-ui-info-2d-buildcat/skip-comparison.json` → `all`.
The Windows-only DXGI memory probe is `platform`; texture-ceiling probe is `opt-in`.
This supersedes the old packet's two-GPU-skip expectation only. It does not waive
unknown skips or convert the two Metal magnifier failures into allowed GPU skips.
These adapter tests are NOT selected for this packet. No full gate is repeated
merely to correct its policy interpretation.

## 5. Stop and report directly

Target under one hour of measurement/analysis once the display setup is available;
checkpoint with exact blocker if access or original conditions prevent it. Run
only the four assigned case invocations; no retry matrix or code changes.

Return directly to Orchestrator II Mac: candidate full head/tree, executable and
runner hashes, all four results, retained stderr/trace hashes, owner-confirmed
versus observed versus unknown display facts, causal verdict and limitations.
Name CI as reused/none found for this unchanged candidate, never manufacture a
new green run. Verify candidate and kernel worktrees remain unchanged.
Keep a compact repo-relative handoff plus hashes in ignored cache; the Mac
orchestrator writes the tracked review and board. No executor reviewer agents,
report commit on the detached candidate, source push or main change is needed.

After independent review, return the verdict/review path to Windows. Windows owns
the follow-up harness fail-fast diagnostic and declared display requirement once
supported by the evidence. No such policy or harness change is implemented here.
The next wildlife input binding remains queued, not abandoned.
