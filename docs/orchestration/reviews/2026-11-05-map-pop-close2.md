# MAP-POP-CLOSE-2: scoped acceptance and next work

Reviewed 2026-11-05 at published `a50aec3a6b0f6bbeb90fee5525c757f2b58b4da0`.
Code pin `42368dd7`; clean canonical gate source `9bcfbf8b`.
Disposition: **ACCEPTED for the explicitly enabled headless progression slice**,
with two bounded test/report corrections assigned to MAP-SUPPLY-1 S0. No
confirmed P1/P2 production defect was found. This does not enable House updates
by default or change either Windows integration input.

## Evidence checked

The orchestrator read the changed production paths, new tests and retained gate
headers/verdicts. Independent read-only reviewer `/root/review_pop_close2`
inspected K2/K3, raw mutation/reload/replay evidence and the original goal-cell
instructions. Earlier `/root/review_inn_k1` independently checked the Inn routines
and callers against the original DLL. No new build or long test run was needed
for this review; retained evidence is attributed to its original source.

- Goal key OR at AnnoWelt `0x1000414b` is supported by original bytes. The resident
  goal sub-cell endpoint remains an adaptation: the fine-route stage is unread.
- Tutorial BuildCmd pioneer 501 upgrades to 522 at tick 55000, with 256 tools and
  1024 wood spent. Material shortages, missing Inn, no build and decline are
  separate controls. The ledger is town-wide, not solely the new house's.
- Seven reload continuations compare against the uninterrupted world. Fresh
  process replay matches six checkpoints; changed command and changed checkpoint
  logs diverge at entries 50 and 1106 respectively.
- Debug: 1685 selected, 1676 executed, 0 failed/empty/incomplete, 9 skips
  (3 platform, 4 opt-in, 2 explicit ANNO_TEST_V4 prerequisite skips classified
  `other`). ASan/UBSan: **275 selected, 274 executed, one opt-in skip**, no failures
  or sanitizer reports. Do not restate this as 275 executed passes.
- Gate source is clean `9bcfbf8b`; later changes are documentation. The failed
  first Debug gate remains a failed evidence attempt, not part of acceptance.

## Bounded corrections; no repeated full campaign

1. `population_progression_test.cpp` at reviewed lines 383 and 498 dereferences
   `transition(Upgrade)` before requiring a non-null result. Bind the pointer,
   REQUIRE it, then dereference. K3g's retained mutation run includes a SIGSEGV in
   the second case. Its separate intended assertion also fails, so the mutation
   is detected, but the crash is poor diagnostic evidence. Recheck both legal
   cases and the missing-upgrade control: the latter must fail at the named
   assertion without crashing. Do not rewrite the historical mutant log.
2. `population-live.md` §23 baseline comparison still says 39 cases without its
   opt-in skip. Correct to 39 selected, 38 executed, one opt-in skip, 0 failed,
   128463 assertions on each side. The orchestrator's isolated `e6661d2f` contains
   this exact wording change; applying the line directly avoids importing the
   separate ECON-BOOK branch. No rerun is needed for the report correction.

## Identity and limits

The push was verified against fetched `origin/task/MAP-2-original-like` at
`a50aec3a`. **128 / timeline 5 is now published.** The earlier permission to amend
an unpublished layout has ended. Future incompatible state, rules/registry or
draw semantics need the appropriate new identity; preserve old test records.
The historical statement that 128 was unpublished must be scoped to the time of
the gates, followed by the publication pin, not left as current guidance.

One house in one town is the demonstrated upgrade slice; another scenario also
has a pinned upgrade but is not a general progression proof. Settlers→citizens,
production replenishment, multiplayer scale and live-window progression are not
established. The stage bit at tick 2500 comes from creation residents, not from
the later upgrade. Original-runtime endpoint fidelity, Windows/Linux builds,
full-suite sanitizers and viewer/GPU checks remain NOT_RUN for this candidate.
Shared modules were exercised indirectly by selected sanitizer tests; that is
not proof that every changed branch of those modules was covered.

Next: MAP-SUPPLY-1, recurring small-farm food/alcohol supply through transport and
residents. Income/upkeep is separately owned by the orchestrator's ECON-BOOK-1
subagent. Keep `Rules::house_updates` false by default.

The S0–S6 dispatch and this review record were independently read by
`/root/review_pop_close2`: approved, no blocking task defects. Its reminder about
initial stocks masking consumer recovery is included explicitly in S5.
