# WILDLIFE-LOAD-FIX-1 — correct the load-time contract and close helper CI

**RELEASED 2026-11-18 after WILDLIFE-LOAD-1 local review and independent dispatch review.**
One 2–4 hour packet for EXECUTOR II MAC. Fix the independently verified loader
arithmetic defect and put the preserved pure helpers on an accepted build base.
This is still unwired simulation code; no wildlife population or hunting claim.

## Intake and reads

Create `task/WILDLIFE-LOAD-FIX-1` from accepted
`2f9e4c0bf84cf09998a43c7ff8098fa1522f3a4e`. Preserve every existing task branch.
This packet explicitly authorizes cherry-picking the five original WILDLIFE-1a
commits, in order, onto that new branch:

1. `75525fcc02f4c2894dae627d893c864db602742a`
2. `56762e421914851e669e3de0345b7311e10af51c`
3. `64ed5655043abd26db0ab6bf6600edbdad240b80`
4. `054fd196871526be41355bddca94c512b5046576`
5. `756e008b8027cf9186b4e79e3ae2e76b1dab4c29`

Resolve only mechanical CMake/append-only REPORTS/lint-list conflicts, retaining
both sides' independent entries. Compare helper source/tests with the preserved
candidate before editing; document any substantive conflict and stop that intake
for review. Do not merge main, rebase old branches, or intake KBEWOHN/DOCS/decoder
candidates. Their notes may be read by pinned ref. No force-push/main push.

Read CLAUDE, PLAN0.1–0.3, TASKS owner rules/section1, testing evidence contract and
README; ADR0006; `docs/re/wildlife.md` sections3/11; `docs/sim/wildlife-decisions.md`;
`tools/wildlife_ref.py`, its contract and `fixtures/synthetic/sim/wildlife_vectors.txt`;
`src/sim/include/anno/sim/wildlife.hpp`, `src/sim/src/wildlife.cpp`,
`src/sim/tests/wildlife_test.cpp`; the preserved1a review and its subsequent
load-domain correction; WILDLIFE-LOAD's boundary note at frozen `f1e245d2769306451d4c2079aadf6a64be93defd` (read-only; no source intake).

## F0 — pin the error with independent expectations

Matching AnnoWelt SHA256
`f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`,
VA0x100946e9–0x10094705: zero-extended u16 island field, multiplier0x10624dd3,
high product shifted5 plus sign correction. All65536 inputs equal floor(n/500).
The old /1000 claim was false. Recheck that narrow span, then add a failing
boundary test before fixing the helper: legal divided bases66/131 and refused132.
Retain base65+jitter31=>96 as an ordinary legal example, not the maximum.
Raw499=>0,500=>1,1000=>2,33000=>66,65535=>131 distinguish caller conversion.
Do not infer physical time units or globally replace the separate sweep arithmetic.

## F1 — minimal pure correction

Correct docs §§3/11 and the pure-helper contract/comment, reference's load-domain
limit, C++ helper guard and vector adapter. Add a small pure typed conversion of
raw u16 stamp to divided load base, with no World/RNG dependency, so the wrong
constant is checked in executable production code before a future caller exists.
Retain the existing already-divided load API; do not silently change its units.
The maximum base plus jitter is162. Preserve goods override, flag-bit semantics,
exact consumed-draw counts and all other decision families.

Extend the independent reference/hand-derived vectors to cover raw conversion
and divided-base boundaries. An exhaustive synthetic check may compare original
multiply/shift arithmetic to division over65536 values; the C++ conversion should
use clear integer division, not an obscure copied multiplier. Python must not
obtain its answer from C++. Record exact new counts instead of retaining the old
65-vector claim. Historical receipts stay historical; new vectors are the only
intended fixture change. Native/replay/pins/default identities stay unchanged.

## F2 — proof the checks distinguish the fault

Keep a legal control green, then show restoring the old helper cap65 fails the
legal66/131 assertions; replacing the raw divisor500 with1000 fails the raw500
boundary; allowing132 fails the rejection assertion. Check jitter disabled/enabled
and draw exhaustion on corrected high bases. Each deliberate fault must compile
and fail the named assertion, not merely a crash. Isolate mutations in owned
scratch builds; restore and rebuild before the gate. Reuse existing unaffected
mutation evidence rather than running the entire historical matrix again.

## F3 — focused verification and compatibility

Run canonical Debug and ASan/UBSan for all wildlife helper tests and required
reference/contracts, plus lint/self-test and the existing partial pin-contract/
identity selection. Quote counts by selection with skip policy. Show no runtime
callers, no initializer, state-field, registry, serialization, scheduling, default
or draw-site changes. No pin regeneration or long sailing/render/corpus run.
If that premise fails, stop and ask for an affected verification scope.

Available warning checks must cover changed C++ including tests. Freeze source,
ordinary-push your own task branch and inspect exact-head hosted CI. This repaired
base replaces the inherited CI blocker, not its historical failure record. A new
owned CI defect gets only the necessary correction, affected tests and re-review;
runner unavailability is NOT_RUN and needs a narrow authenticated retry, never
an empty commit or fabricated pass. Do not touch coordinators or other lanes.

## F4 — handoff and stop

Write compact evidence and REPORTS: intake/base/final head+tree, exact CI, decisive
raw boundaries, preserved failure, new counts, negative controls, all moved files,
reuse justification and NOT_RUN. Public evidence uses relative paths and hashes;
no original bytes/listings/data tables or raw logs. Record Decisions and reasoning.

Send READY FOR REVIEW directly to the Mac orchestrator; it supplies independent
read-only review, including primary evidence, tests and receipts. No executor
reviewer agents. Stop before tags, ScenarioSpec, World import, fauna sweeps,
materialization or hunting. No owner policy is needed for the verified /500 fix;
unresolved global load order and mode provenance remain later integration work.
At six hours return a clean bounded checkpoint rather than expanding the task.
