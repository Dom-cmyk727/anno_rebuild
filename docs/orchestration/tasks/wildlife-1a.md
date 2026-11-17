# WILDLIFE-1a — pure wildlife decisions, before any world integration

**Wave 1.2; WRITTEN, not yet released.** EXECUTOR II MAC, after the TOWN-START-1 review
and an explicit Mac-orchestrator release. About 3–4 hours; one concern. Branch
`task/WILDLIFE-1a` from freshly fetched main at release, with its base recorded.
No reviewer subagents; the orchestrator reviews the frozen result.

## Read first

CLAUDE.md; TASKS.md owner rules and section 1; testing/evidence-contract.md and testing/README.md;
PLAN.md sections 0.1–0.3; `docs/re/wildlife.md` sections 1–11 and all four
`docs/orchestration/reviews/2026-11-12-wildlife-re-1*-review.md` files;
`tools/wildlife_ref.py`, `tools/wildlife_ref_contract_test.py`,
`fixtures/synthetic/sim/wildlife_vectors.txt`; the existing fisher pure-helper/test structure;
ADR 0005, ADR 0006 and `docs/sim/state.md` section 9.7. The 65-case reference is an integer
decision oracle, not an animal lifecycle or a full-tick oracle.

## Steps

1. **Freeze the contract.** List the ten existing record kinds (density, habitat, replenish,
   create, load, animal, walk, route_old, wolf, prey), their input/output units and widths,
   consumed-draw semantics and unresolved boundaries. Run the existing reference and its
   contracts unchanged; record the actual vector count. Resolve discrepancies before coding.
   Hand-derive one discriminating boundary per decision family using the cited original
   addresses/data; retain the distinction between evidence, adaptation and unknown.
2. **Implement pure typed C++ helpers** in sim, with no World, clock, mutable globals or I/O.
   Inputs supply resolved facts, tables and a bounded sequence of raw u32 draws. Outputs
   include decisions and the exact draw count. Return a typed unsupported/invalid result for
   documented unresolved cases or exhausted supplied draws; never fabricate a result or retry
   forever. Runtime world PRNG, fauna sweeps, scheduling, spawning, movement and hunting stay unwired.
   Keep content mappings supplied as data where applicable; original fixed widths are decoded
   semantics with evidence, not new limits on the world's registry or player count.
3. **Exercise all 65 existing vectors through the actual helpers.** The C++ vector adapter
   must not call Python for its answer or echo expected fields. Require positive executed counts
   by record kind and compare every specified output, including draws consumed and list order.
   Missing/unknown records and empty input fail; do not weaken the reference or silently skip
   a difficult record. Add malformed/unsupported cases for the published OQ boundaries.
4. **Small negative controls before broad gates.** Keep legal controls green, then show the
   intended assertion fails for density 13/14, collision draw/swap-pop order, byte-wrapped
   facing/attempt, an idle modulus-one draw, a draw before quantity override, and short versus
   extended predator membership. Use isolated mutations with exact source/build provenance,
   restore and rebuild cleanly afterward. A crash or build failure is not the intended kill.
   Do not broaden this into another mutation framework or repeat unchanged research campaigns.
5. **Focused verification and handoff.** Register the helper tests and required reference checks,
   run canonical Debug and ASan/UBSan selections, format changed C++, and perform the available
   warning checks. Run `tools/pins.py check` normally (no regen): zero moved pins and no fixture
   identity/layout/PRNG/default change. Record unavailable target platforms as NOT_RUN.
   Ordinary-push the task branch and inspect hosted CI at the exact reported head.

## Boundaries that must survive the port

Replenish selection order is not the later prepended-record visit order. Creation ends before
manager attach callbacks. Load's timestamp base is already divided. Walk outputs radius/angle
indices, not float positions or reachable paths. Completion receives post-action quantities and
callee outcomes, not a simulated action. Predator 503 uses the short list; it is not admitted prey.
OQ-572/574/585 and transitive/float/lifecycle gaps stay explicit. No section/tag reservation or
identity change is authorized by this pure-helper packet. If one appears necessary, report QUESTION.

## Done and stop

The acceptance table records 65/65 comparisons, all ten kinds, targeted negative controls,
zero pin moves, clean final source and exact hosted CI. Report READY FOR REVIEW in about 60 lines
with Decisions and reasoning, preserved failures, counts, head and compact evidence hashes.
Raw logs remain in cache; no absolute host paths, original tables or disassembly in git.
Stop before wiring a single helper into World. Do not start WILDLIFE-1b, change defaults,
force-push, push main or alter another lane. Past six hours, report a clean bounded checkpoint.
