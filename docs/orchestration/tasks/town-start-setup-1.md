# TOWN-START-SETUP-1 — forward existing extension rules at ship-start creation

**RELEASED 2026-11-17 to EXECUTOR II MAC.** A small prerequisite to TOWN-START-1,
about 1–2 hours. This is option B from the executor's early question: keep the production
setup seam separately committed and reviewed before the measurement. No new gameplay policy.

**Base:** `b5c0ada209d94a0082bc88d793c917c7222e6a21`, the fetched main pin already recorded
by the executor. Branch `task/TOWN-START-SETUP-1`. The empty, unmodified TOWN worktree may be
reused by creating this new branch there; preserve the unstarted TOWN branch. Do not reset,
rebase or edit another worktree. Measurement implementation waits for this packet's review.

## Read and confirmed gap

CLAUDE.md, the evidence contract and testing README; the TOWN-START-1 packet;
`src/sim_load/include/anno/sim_load/ship_start.hpp`, `src/sim_load/src/ship_start.cpp`,
`src/sim/include/anno/sim/extension_context.hpp`, `World::create` in world.hpp/scenario.cpp,
`src/sim/tests/ship_start_load_test.cpp`, and `docs/sim/ship-start-integration.md`.

The orchestrator independently confirmed: load/install_ship_start accept base Rules but no
ExtensionContext. The load path invokes World::create without its existing final extension
argument. FisherRules is already header tag 1, default enabled=0, with values >1 invalid.
This packet forwards that existing context; it does not implement another feature or mutate
an existing world's rules. No WorldTestAccess workaround is authorized.

## Steps and proof

1. Add an optional final `const sim::ExtensionContext&` argument, default empty, to both public
   ship-start entry points without changing the order or meaning of existing arguments.
   Forward it through install -> load -> World::create. Reuse World::create's validation;
   do not add a second validity policy, remap errors, silently clamp values or hardcode fisher.
2. Add focused public-path tests, using an existing valid original-file fixture:
   - Old omitted argument and explicit empty context produce identical saved bytes/hash,
     default-off fisher and the existing expedition baseline. Existing pins do not move.
   - Explicit enabled=1 reaches the created world and survives normal native save/reload.
     Check both load and successful install, not just a helper or the context object itself.
   - Explicit enabled=2 is refused by load and install; failed install leaves the pre-existing
     target's serialized bytes and hash unchanged. Preserve all existing loader refusals.
3. Demonstrate the distinguishing assertion: temporarily dropping context forwarding fails
   the flag-on test while the default control passes. Check the install forwarding edge too;
   a mutation dropping only that edge must fail the install case. Restore and rebuild before gates.
   Invalid-context refusal prevents a success result from hiding a dropped or clamped value.
4. Update only the loader API/contract description and a short REPORTS entry. Keep the shipped
   default, tag registry, section/global versions, PRNG timeline, candidate manifests and original
   content unchanged. Do not propagate a new UI switch or change a production caller's choices.
5. Run canonical focused loader/extension/save tests in Debug and ASan/UBSan, existing affected
   ship-start caller checks, formatting and available warnings, then normal pins check (no regen).
   Reuse unrelated lanes with the explicit reason: optional setup argument, default path unchanged.
   Record native Windows/Linux lanes NOT_RUN here; hosted CI must be green at the pushed head.

## Stop and handoff

Only the loader declarations/definitions, focused tests and their needed registration, contract
doc and REPORTS are in scope. Any need for another state representation, new fish placement,
default flip or additional runtime repair is a QUESTION, not part of this seam.
No reviewer subagents. Ordinary-push only the task branch; send READY FOR REVIEW directly to the
Mac orchestrator with exact head, CI, classified counts, mutation results, zero moved pins and
Decisions and reasoning. Raw evidence remains private cache, all committed paths repo-relative.
The orchestrator reviews this prerequisite, then explicitly releases TOWN-START-1 on its accepted
head as a local stack or on the landed equivalent. Do not wait for or perform an unauthorized main merge.
