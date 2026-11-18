# MIL-RE-1 — first military command and soldier read

Wave 1.4, RELEASED 2026-11-18 after NET-ADR-1's documentation acceptance. One bounded
research packet, approximately 3–4 hours. Deliver the first verified command/tick map,
not a combat implementation or a complete military specification.

Start `task/MIL-RE-1` from main pin `b5c0ada209d94a0082bc88d793c917c7222e6a21` in your
owned, clean worktree after the previous report. Preserve NET-ADR-1 and WILDLIFE-1a.
No merge, rebase, pending CI-repair intake, main push or force. Ordinary-push your own
branch at its checkpoint. No reviewer subagents; the orchestrator reviews the result.

## Read first and establish the boundary

- CLAUDE.md, AGENTS.md, PLAN goals, TASKS.md owner rules, testing/evidence-contract.md,
  testing/README.md and orchestration/mac-lane-2026-11-17.md wave 1.4.
- Existing military references in `docs/re/`, scenario SOLDAT layout in
  `docs/formats/sze.md`, and the figure class/route mappings currently loaded by sim.
- `docs/UNITS.md` Parts C and D2–D3 and OQ-753 in `docs/open-questions.md` from the
  preserved reference pin `95897a55bc939a185766b7207e69d098997cdb1d`. These files/rows
  are absent from the named main baseline: read them using `git show <pin>:<path>`.
  Do not merge that UI candidate or copy its whole question register. Its render gate
  failed; its research notes are leads, not primary proof. Recheck claims you rely on.
- Relevant symbol/RTTI indexes and existing disassembly helpers. Use the existing
  private tool environment where possible; avoid scanning the whole home directory.

## M0 — record a small evidence map

Hash the actual original modules read. Record tool/version, source baseline and function
boundaries in private `cache/mil-re-1/`. Build a compact table of question, primary address,
observed predicate/call, confidence and unread dependency. Distinguish static evidence,
inference and runtime observations (NOT_RUN on this Mac). Do not copy original listings,
data tables, assets or host paths into git.

Start from AnnoWelt soldier vtable `0x100c561c`, subclasses `0x100c59e8`, `0x100c5bd8`,
`0x100c5804`, and ground-figure vtable `0x100c35a8`. Treat these as leads until verified
against this module's bytes. Establish constructor/class identity and the overridden slots
needed for one order and one update. Do not read every slot simply to fill a table.

## M1 — trace one attack-order path

Follow a concrete player order from its AnnoFrame handler to the command record/sink and
the receiving simulation handler as far as the bytes support. Identify target kind, issuer
and figure selection, record fields/widths and the switch from order to route/job. Trace
ownership/target checks in their actual order. Do not infer authentication from a player
field or reuse ship command numbers by analogy.

If an indirect edge cannot be resolved, show the exact producer, slot and unresolved
consumer; continue independent soldier reads. Keep group selection, formations and AI
orders explicitly separate unless reached and verified. One worked synthetic example
should distinguish the decoded command/target rule from its plausible wrong reading.

## M2 — trace the corresponding soldier update

Identify the update/dispatch slot and the smallest verified transition chain between idle,
walking and attack. Record relevant route/job identities, time units, state widths and
failure/target-disappearance handling. Follow damage/death only as far as this bounded chain
requires; leave healing, theft, boarding and every subclass's full behavior as named later
reads. Verify function ends so a following constructor cannot be mistaken for a callee.

For each decisive mask, comparison or timed transition, derive one boundary example and
the different result under the likely wrong interpretation. Reuse a bounded executable
reference only where it adds real discrimination; do not create a generic emulator framework.
No guessed damage formulas, no claim that static evidence establishes original runtime parity.

## M3 — publish the implementation boundary

Write `docs/re/military.md` with the verified call graph, addresses, compact predicate table,
worked examples, unread edges and a realistic estimate/order for the remaining reads. State
which subset of OQ-753 this answers; do not close the whole question. Recommend the smallest
future contract/implementation packet that the evidence supports, and its prerequisites.
Separate original fixed layout widths from configurable engine data/limits and keep future
mod support possible without designing a new subsystem here.

Append a concise REPORTS.md entry: decisions/reasons, module hashes, methods, exact checks,
CI head/run, limitations and links. Raw disassembly and detailed notes stay in ignored cache;
commit only compact findings and hashes. Allowed edits are that note, the bottom report and
at most a small necessary index link. No src/, fixtures, original files, state/PRNG identity,
rules/defaults, coordinator, test workflow, UI/Windows, TASKS.md or STATUS.md edits.

## Review and stopping criteria

Check all new links/addresses, distinguish inherited claims from newly verified ones, inspect
the diff for original material/host paths, then commit and push the bounded report. Observe
exact-head hosted docs CI; no C++ or long game-file campaign for documentation alone.

Report READY FOR REVIEW directly to the Mac orchestrator with full head, CI run, changed files,
evidence hashes, verified chain and remaining edges. Stop at this research boundary even if
more military code remains unread. If a true owner-policy decision appears, send the choices
to the orchestrator and stop only the dependent work. Do not implement combat, start network
work, enable wildlife or wait for a policy answer while unrelated reads remain useful.
