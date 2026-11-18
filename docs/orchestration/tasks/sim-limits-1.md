# SIM-LIMITS-1 — one definition for freight compatibility bounds

Wave 1.7. RELEASED, 2026-11-18, directly to EXECUTOR II MAC after OQ-562-1 acceptance
at b7046e5e (exact-head CI44282873819, all12 jobs passed). This replaces the parked
2026-11-12 Windows/state138 wording.
Owner's Mac-lane handover assigns this bounded packet to EXECUTOR II MAC. About 2–3 hours.

Create task/SIM-LIMITS-1 from accepted source 9edb151c414246d306194a703db71475affbdad4,
CI44266114323 (all12 jobs passed), at an owned clean checkpoint. It is an independent new
branch; do not merge/rebase preserved OQ/wildlife/ore work or substitute moving main. This
is a reviewed candidate base, not a claim that it landed. Ordinary-push your task branch,
never main or force. No reviewer subagents; the orchestrator reviews the frozen handoff.

## Read first

CLAUDE.md, AGENTS.md, PLAN goals, TASKS.md owner rules, testing/evidence-contract.md,
testing/README.md; MAP-INTEGRATE-2's review finding E5; docs/sim/state.md, ship-trade-routes.md
and the relevant command framing contract. Inspect current reject.hpp, ship_step.hpp,
freight_state.hpp, figure.hpp, state_fields.hpp, freight.cpp, config.cpp and codec.cpp.
Read the existing freight, codec, config, stream and integration tests before changing them.

Keep a short private cache/sim-limits-1/CURRENT.md with task/ref, branch/base, steps, next check,
owned jobs and evidence paths. After compaction, reread that note and this packet; closed
historical gates do not replace the current task.

## S0 — pin the current contract, not historical line numbers

Write a compact acceptance table and inventory all reached copies of these four bounds:
stored freight rejection, navigation failure, command framing sizes, and HarvestState's
split serialization guard. The baseline state identity is139, not the old packet's138.
Record actual section/PRNG/wire identities and leave them unchanged.

Important correction to the old premise: Reject::NoDepositInReach is explicitly RESERVED
and never produced. Being the last declared enumerator does not prove it belongs to the
valid freight stored-reject domain. Inspect all producers/consumers and preserve the current
valid domain, including reserved/unknown rejection, unless a concrete contrary contract is
found. Do not silently widen accepted saves under a claim of a behavior-preserving refactor.

## S1 — centralize the two enum domains

Give freight validation and the native reader one shared rejection-domain definition next to
the authoritative enum/domain information. Distinguish defined, reserved and valid stored
codes. Remove stale copied endpoint decisions in the two consumers. Ensure a future appended
valid code either participates automatically in the shared definition or triggers a build/test
failure requiring deliberate classification; an unchecked alias to today's endpoint is not enough.
Use the smallest existing project mechanism, not a generic enum/reflection framework.

Likewise derive the navigation-failure domain from NavFailReason in ship_step.hpp, including
its existing zero/no-failure representation, rather than two unrelated literal '>3' checks.
Preserve encodings and error behavior. Add legal endpoint, reserved and first-invalid checks
through actual validation/load paths, not only calls to the new helper. A deliberate stale
consumer/omitted classification must fail its intended check while the legal endpoint passes.

## S2 — share command framing arithmetic without conflating save layout

The command payload is 12 + 20*stops + 13*actions, bounded by the existing65535-byte framing.
Centralize these facts and checked arithmetic for freight.cpp, config.cpp, state_fields.hpp
and codec.cpp's size/decoder checks where they represent that same format. Preserve caller-
specific empty/count constraints and overflow-safe refusal. Keep the current maxima:
3276 stops and5038 actions for the one-stop upper-bound calculation. Hand-derive boundary
examples and mixed stop/action cases; huge counts must refuse without allocation or wrap.

Do not replace native layout's distinct24-byte stop minimum with the command's20-byte record.
They describe different encodings. Trace each occurrence to its consumer before replacing it.
Do not change framing, native bytes, limits, field order, aliases, command tags or mod policy.
The wire limit is a format contract, not a new fixed engine world capacity.

## S3 — make the split HarvestState guard truthful

The aggregate has nine members: seven are handled by fields(HarvestState), with the two
mason goal sub-cell members serialized conditionally by fields(Figure). State that split in
the guard/message and bind its counts to the actual layout obligation using a minimal check.
Do not start serializing those two fields unconditionally, drop them, or alter byte order.
Prove a nonzero mason sub-cell round-trip and an ordinary harvester's unchanged handwritten
bytes. A temporary added unhandled member or omitted conditional field must be caught.

## S4 — focused verification and stop

Cheap checks and hand-derived boundaries first. Then canonical focused Debug and ASan/UBSan
selections for the affected freight/codec/config/state/identity paths, with originals required
when selected. Run sim_stream.py --check and the existing integration stream references without
writing mode. Zero pins, golden bytes or recordings may move. Reuse unaffected source-pinned
checks; no full-town experiment, full-suite campaign or pins regeneration. Record exact selected,
executed, skipped, failed/empty/incomplete and sanitizer counts. Preserve initial failures and
mutation controls, restore source byte-for-byte and rebuild before final positive gates.

Freeze source during gates, format changed C++, ordinary-push and observe exact-head hosted CI.
Mac local results do not claim native Windows/WSL/real-adapter or original-runtime coverage.
If a genuine compatibility/behavior decision is necessary, finish independent work and send the
exact decision to the orchestrator; do not authorize a version/default change yourself.

Allowed scope: the shared small definitions, their named consumers and focused tests; a compact
contract clarification if necessary; bottom REPORTS entry with decisions/reasons, evidence hashes,
identity/fixture checks and Windows-consumer delta. No unrelated cleanup, workflow/coordinator,
assets/maps, wildlife, OQ repair, UI or network work. Do not edit TASKS.md, STATUS.md or mac-lane.md.
Raw logs stay private cache/sim-limits-1; no host paths or original material in git.

Send READY FOR REVIEW directly to the Mac orchestrator in the Orchestrator II Mac chat
(owner-authorized reporting) with full head, CI,
acceptance table, retained failures, exact
counts and remaining limits. Stop there. Completion requires unchanged supported behavior and
bytes plus a demonstrated guard against the next stale bound; a helper-only green test is not
proof. DOCS-SIM-1 remains queued for a later explicit dispatch.
