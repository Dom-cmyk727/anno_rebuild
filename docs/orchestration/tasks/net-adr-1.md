# NET-ADR-1 — reconcile the LAN contract and prepare owner choices

Wave 1.3. RELEASED 2026-11-18 to EXECUTOR II MAC at the reviewed WILDLIFE-1a checkpoint; release is recorded in
mac-lane.md. One documentation concern, about three hours. This packet implements no network,
simulation, socket, wildlife or gameplay code. It can proceed independently of town/wildlife
landing and the separate TOWN-CI-CLOSE-1 repair. Preserve both frozen gameplay candidates.

Use branch `task/NET-ADR-1` from main pin `b5c0ada209d94a0082bc88d793c917c7222e6a21`.
Fetch for visibility; do not substitute a moving base, merge main or import pending repair commits.
Reuse an owned clean worktree only after its current report/gates are finished. Ordinary-push your
task branch at the checkpoint; never push main or force. No reviewer subagents: the orchestrator
reviews your frozen result. No owner decision is delegated to you.

## Read first

- CLAUDE.md, AGENTS.md, testing/evidence-contract.md and testing/README.md; TASKS.md owner rules.
- `docs/orchestration/mac-lane-2026-11-17.md`, wave 1.3, and
  `docs/orchestration/remaining-work-2026-11-16.md`, sections 5 and 7 (D-9/D-10).
- `docs/adr/0003-network-transport.md`, including its measured/synthetic limits and unresolved
  join/ownership boundaries; `docs/adr/0005-edition-neutral-content.md` and
  `docs/adr/0006-save-sections.md`, including amendments and their decision status.
- `docs/plan/lan-endless.md`, sections 4 and 7; PLAN sections 0.1–0.3 and the five-player MVP.
- `docs/sim/state.md`, especially section 9.7; `docs/sim/commands.md` if present, otherwise the
  command contract indexed by state.md. Read the current command/session interfaces and schema,
  section/tag, content/rules and PRNG identity implementations before naming implemented seams.
- The existing transport/join/reference tools cited by ADR 0003. They are models, not proof of
  a working C++ network. Inspect their inputs/outputs only where relied on; no broad remeasurement.

## N0 — inventory the contradictions before editing

Make one compact table in the report: current statement, authoritative replacement or unresolved
owner choice, proposed location, and validation. At minimum cover raw original-file hashes versus
canonical content identity; one global state number versus section/schema identities; language and
graphics outside simulation identity; five human seats versus historical six-seat/computer-player
assumptions; and dropped-seat handling where no AI is implemented.

Distinguish the original game's behavior, accepted project decisions, proposals, measured model
results and unimplemented capabilities. Preserve historical measurements with their original inputs.
A hypothetical budget or synthetic link result must not become a LAN measurement by relabelling it.

## N1 — reconcile handshake and compatibility

Update ADR 0003 to reference ADR 0005's canonical simulation content identity, not raw asset-file
equality. Separate schema/section base versions, PRNG semantics, recognized extensions, active rules,
content and session configuration; say which mismatches refuse joining and how the differing part is
reported. Follow ADR 0006's actual contract; do not invent permissive unknown-tag handling or an
automatic migration. Graphics/language differences alone must not defeat the edition-neutral goal.

Trace every named implemented identity to the current code/interface. If an identity component or
comparison is still missing, label it a required implementation seam. Do not claim mixed-edition
network parity from file-reader parity. Explain how later mod overlays participate in deterministic
content/rules identity without designing a new mod system here.

## N2 — make the session model consistent with the MVP

Describe five human participants as the MVP workload, not an engine capacity limit. Keep configurable
seat and workload limits separate from wire-format widths and measured operational budgets. Trace
join, input ownership, disconnect, reconnect and host-role transfer in a short state/event table.
Separate transport acknowledgement, matching snapshot/hash and actual command authority; do not
grant a seat merely because one message arrived. The authenticated session/seat mapping determines
the issuer, not a packet's claimed player number.

Do not silently implement or promise AI takeover. Retain the overall host-migration requirement while
clearly separating its MVP timing decision. Existing guarantees and their known gaps remain visible.
Describe the handling of stale incarnation traffic, bounded retained work and refusal/timeout paths
by citing the established ADR model, not by introducing new protocol constants.

## N3 — one page of owner decisions

Write `docs/orchestration/decisions/net-adr-1-owner.md`: concise options, a recommendation with its
tradeoff, what each choice blocks, and a place for the owner's answer. Cover D-9/D-10:

- ADR approval, UDP port and proposed operational limits: distinguish already decided transport
  architecture from remaining parameters; do not reopen accepted choices without a contradiction.
- Venetians/natives in the MVP; what a dropped human seat does without an AI; whether host migration
  is in the MVP or a later milestone.
- Fish on the selected LAN map and construction-stage enforcement, with current engine capabilities
  and dependencies rather than unverified promises.

Record the owner's established ship-start preference and peaceful relations/shared visibility/no
alliances as existing requirements where relevant; do not select a map or flip living defaults.
If a historical decision conflicts with a newer instruction, show both and ask the owner to resolve
the remaining policy. Never mark a recommendation accepted. Leave ADR status Proposed until approved.

## N4 — bounded implementation handoff and consistency check

End the ADR with a compact dependency order for future packets: headless protocol/state model,
identity/ownership checks, loss/reorder/duplicate tests, snapshot/catch-up, reconnection, socket adapter,
and live multi-peer acceptance. Respect the Mac headless/Windows socket-and-viewer split and mark
which steps await the owner's answers. No estimated throughput claim without evidence, no detailed
new framework and no implementation in this packet.

Check every edited cross-reference and all current handshake/MVP statements across ADR 0003 and
the decision page. Summarize remaining historical mentions rather than mechanically replacing
numbers in old measurements. Changes should be focused amendments with clear supersession, not a
rewrite of the long measurement history. Confirm source/fixtures/identities/assets are unchanged.
Run the repository's applicable document/format checks and exact-head hosted CI. No C++ campaign is
needed for a docs-only packet; report any inherited CI failures separately without repairing them.

## Done and stop

Allowed edits: ADR 0003, the new owner-decision page, a narrowly necessary link/correction in
lan-endless.md, and the bottom of REPORTS.md. Do not edit ADR 0005/0006's accepted policy, TASKS.md,
STATUS.md, mac-lane.md, source, tests, fixtures or any other lane. If a larger change is necessary,
report the exact contradiction and stop only that dependent part.

Report READY FOR REVIEW directly to the Mac orchestrator: full head, CI, changed files, contradiction
table disposition, owner decisions still open and a short proposed next implementation boundary.
Keep raw notes private; no original files, packet dumps, host paths or credentials in git.
Stop before implementation or changing ADR approval status. This packet prepares owner decisions;
it does not authorize network implementation, wildlife integration or main landing.
