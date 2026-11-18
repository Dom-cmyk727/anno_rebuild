# WILDLIFE-1a — pure wildlife decisions, before any world integration

**Wave 1.2; RELEASED 2026-11-18 to EXECUTOR II MAC** after the
[town local review](../reviews/2026-11-18-town-start-1-review.md).
Town landing is not a dependency: this packet does not consume its loader or measurement changes.
About 3–4 hours; one concern. Branch `task/WILDLIFE-1a` from the verified main pin
`b5c0ada209d94a0082bc88d793c917c7222e6a21`. Fetch for visibility, but do not silently substitute a
moving base. Preserve the frozen town branch and evidence. Reuse an owned clean worktree if suitable;
use distinct build directories so town's retained binaries and logs remain available for review.
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
   warning checks. Run `tools/pins.py check` with an explicit affected/quick identity selection
   (no regen), including its required `pins_contract`; label that result **partial**, with the
   selected names. Check the base-to-head diff and fixture hashes: zero moved pins and no fixture,
   identity/layout/PRNG/default change. Do not repeat the unchanged long corpus campaign merely
   because new, unwired functions were added. The reuse argument must show that existing runtime
   code never calls the helpers, no global initializer was added, and existing state/serialization,
   registry, scheduling and random-draw paths are unchanged. If that is false, stop before widening
   scope and request an appropriate verification selection. Record target platforms as NOT_RUN
   where unavailable and distinguish reused full-campaign evidence from the new partial check.
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

## Inherited CI failure, recorded 2026-11-18

Town source `f36c22fa` hit GCC Release `-Werror=null-dereference` at
`src/sim/include/anno/sim/freight_state.hpp:33`, while compiling `freight.cpp`, in CI
[44255019025](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44255019025).
Those files, the sim CMake file and CI workflow were unchanged from the accepted setup source.
Clang ASan also failed in unchanged test-support compilation on libstdc++ 15's `ciso646`
C++20-header diagnostic. Town's separate helper-linkage error was corrected in its own branch;
it is not part of this main base. The root causes of the base failures are not established.
This is not permission to suppress warnings, call CI green,
retry until green, or add a freight repair to wildlife. If the failure remains at your base/head,
preserve it and return READY FOR REVIEW / CI BLOCKED after the scoped work. Exact-head green CI
remains required for acceptance and landing. Independent gameplay development need not wait for
that separate build issue to be resolved.
