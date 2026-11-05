# W4-OPENING-1-CLOSE: independent review of the closeout package — ACCEPTED WITH FOLLOW-UPS

Reviewed by an independent read-only agent, dispatched by the Claude orchestrator/executor session on the Windows
PC, per the mandatory evidence contract's "independent review and completion" protocol
(`docs/testing/evidence-contract.md`).

**Identity:**
- branch `task/W4-opening-1`, worktree `E:\anno1503-worktrees\anno1503-W4-opening-1`;
- package: 11 commits, base `02133ade9e66426ab093d8377992aec159367671` → head `3a2da9b92698000416a69053b3d77a8dc569fbb6`;
- the 11 commits, in order: `4d37bfd2`, `837590a8`, `b2e771ca`, `871f719f`, `35777bc0`, `07fa9071`, `a379e06e`,
  `a315b51a`, `86dec376`, `1d34dd97`, `3a2da9b9`.

**What was reviewed:** the frozen diff for all 11 commits, hand-derivation of the lan5 pick fix and the zoom-step
chunk fix against their own tests, hand-verification of all ~12 relocated `iso.hpp` call sites against the prior
inline formulas, the Wine founding-key trace against `OQ-520`, and the raw gate logs (`native-close1a/g/r.*`,
`unix-warnings-closeout-final.log`, `gpuwin-close1-*`, `wsl-w18*`, `wsl-w19.txt`) rather than only the executor's
summaries. `git diff --stat` confirmed zero files under `src/sim` in this package.

## 1. Verdict

**ACCEPT WITH FOLLOW-UPS.** No blocking defect in any of the 11 commits. Per item:

1. **River Q5** (`4d37bfd2`, `35777bc0`) — ADEQUATE. The live gate still prints exactly `0.485714` (17/35) at head,
   matching the docs; a genuinely recorded open gap, not a lowered bar or a masked regression (the other 4 QA
   islands still pass at both shores).
2. **lan5 pick fix** (`b2e771ca`) — ADEQUATE. A real algorithmic fix (the ray is partitioned at its exact `u=k` /
   `v=k` / `u+v=k` crossings instead of marching in 1-height-unit steps that could skip a sub-unit triangle sliver),
   with a regression test that fails without the fix (62/25,069 wrong under the old march; 0/8,486 and 0 on the
   synthetic case with the fix).
3. **Projection cleanups** (`871f719f`, `837590a8`, `a379e06e`) — ADEQUATE. Every relocated formula reduces
   algebraically to the prior inline expression; the two kept-inline sites (the GPU shader, the ground rasteriser's
   rectangle-extent inverse) are named with a reason. Island names print as names; Linux/Apple digests say NOT_RUN.
4. **Wine check of founding keys** (`07fa9071`) — ADEQUATE. The recorded Wine trace matches the code's key mapping
   exactly, with a standalone unit test.
   - **`a315b51a`** (building-tool cursor rotation) and **`86dec376`** (zoom-step chunk build), grouped with this
     item though not pure wording — both ADEQUATE. `a315b51a`'s centred-origin math matches the cited
     `AnnoFrame.dll 0x10043e20` evidence and reuses the already-reviewed sim rotation/coast helpers read-only.
     `86dec376`'s anchor-preserving offset formula was hand-checked against the new test's literal expected values
     and reduces to the old formula when the anchor is the view centre; honestly flagged by its own author as "not
     yet measured in a zoom replay."
   - **`1d34dd97`** — ADEQUATE, and confirmed to be a real fix, not cosmetic: `wsl-w15-build.log` shows the exact
     GCC `-Werror=missing-field-initializers` this commit corrects, with no change to the turn value's actual flow.
5. **Gate sequence** — ADEQUATE, with the opening_controls caveat below. Raw logs for native app/GPU/render, the WSL
   warnings check and the GPU window tests all match their claimed summaries at the declared source head.
6. **`3a2da9b9`, the quit-deadline fix** — ADEQUATE. The non-sanitizer default is unchanged; the 5x factor and
   1200 s figure are backed by the cited measurements (434.4–494.9 s across two independent runs); nothing is
   loosened outside `if(ANNO_SANITIZE)`.

## 2. The `viewer_wiring_opening_controls` flake — not fully settled on one rerun

The executor's rerun (`wsl-w19`, same binaries, no rebuild: 266/266, 0 failed) is consistent with "contention flake,
not a regression." The reviewer independently checked the *mechanism* the executor proposed (that
`opening_session_replay`, now running far longer under ASan, newly contends with `viewer_wiring_opening_controls` as
`-j4` siblings) and found it **does not hold**: both tests carry `PROCESSORS 4` in `src/app/CMakeLists.txt`, so
CTest's scheduler never runs them concurrently at `-j 4` — confirmed directly in `wsl-w18-app.log`, where
`opening_session_replay`'s completion line precedes `viewer_wiring_opening_controls`'s start line with no overlap.
So `3a2da9b9` (which only touches `opening_session_replay`'s own `ENVIRONMENT`) has no plausible causal link to the
`opening_controls` failure via sibling contention.

What the failing run's stderr shows instead is a **sustained** ~50–57 ms/frame cost (not a spike) from ASan's CPU
rendering overhead under `-j4` host-wide contention — a structurally tight real-time polling margin in
`opening_controls` that predates this package, not something this package introduced. One clean rerun is evidence
for "flake, not a new regression," but given the margin looks structurally tight rather than a one-off spike, the
reviewer would not call it fully settled on a single rerun.

**Resolution:** a second confirmation rerun (`wsl-w20`, same binaries) is running now (owner's push/merge is gated
on it, not on this review). If `wsl-w20` also comes back clean, items 1–2 below move from "before trusting" to
"tracked, non-blocking."

## 3. Follow-ups (none blocking this package)

1. Give `viewer_wiring_opening_controls`'s real-time waits the same kind of `ANNO_SANITIZE`-scoped slack that
   `3a2da9b9` gave `opening_session_replay`'s quit deadline, rather than relying on the host staying uncontended.
   Owner/lane: rendering executor, next idle WSL slot. Acceptance: the case passes repeatedly inside a full `-j4`
   ASan batch, not only standalone.
2. Design observation, not required now: a turn-based completion signal (as `dev_script.cpp`'s own `wait turns`
   already does) would be strictly more robust than any wall-clock deadline for these two tests, sanitizer-scaled or
   not — invariant to host speed/load instead of needing a hand-measured multiplier. Worth considering next time
   either test's timing budget needs touching again.
3. Minor: `coast_turned`/`deposit_turned` (from `a315b51a`) hardcode their PRODTYPE list inline rather than via a
   config table. Currently fine — each value is evidence-cited — but worth moving to config if/when moddability of
   coast kinds becomes near-term scope.

No `src/sim`, `src/formats` or `src/test_support` file is touched by this package.
