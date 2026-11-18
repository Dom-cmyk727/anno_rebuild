# ANNOSCENE-ORE-2 — placed ore quantities and treasure sites

Wave 1.5. RELEASED 2026-11-18 after MIL-RE-1's independent review and exact-head docs CI.
One bounded original-code/data research packet, about 2–3 hours. No simulation implementation,
map selection, generated ore insertion or PRNG/identity change is authorized.

Use `task/ANNOSCENE-ORE-2` from main pin `b5c0ada209d94a0082bc88d793c917c7222e6a21` in
an owned clean worktree after the current checkpoint. Preserve previous task refs. Ordinary-push
the checkpoint; never merge/rebase other work, push main or force. No executor reviewer subagents.

## Read first

- CLAUDE.md, AGENTS.md, PLAN goals, TASKS.md owner rules, testing/evidence-contract.md,
  testing/README.md and mac-lane-2026-11-17.md wave 1.5.
- `docs/re/annoscene.md` section 5 and OQ-151 in `docs/open-questions.md` at the baseline.
- The accepted predecessor ANNOSCENE-ORE-1 at `ea023e18386c465e795c33cac6d1f33f46b2a405`:
  read its annoscene.md section 5.1, `tools/annoscene_ore_ref.py`, its public-command contract
  tests and report using `git show <pin>:<path>`. It was stacked by merge ac02d4a2; do not
  merge that candidate or overwrite baseline files with its versions. Extract reference tools
  to private cache only if needed, recording their exact blobs. Setup correction: the named
  b5c0ada2 baseline already contains the same three documentation/tool blobs as ea023e18;
  use them directly. The dispatch chat's claim that they were absent was incorrect.
- `docs/re/mine-deposit-followups.md`, `docs/sim/salt-extraction.md`, existing island/OCCUR/
  BERGWERK parsers and original registry facts where newly relied on. Read selectively; this
  packet concerns scenario placement, not another mining-production rewrite.

## O0 — inventory and distinguish three concepts

Hash original modules/input sets and record source/tool identities. Reconcile the predecessor's
429 disk templates and 1,590 markers with this installation; distinguish the network's 428-entry
list, excluded files and any installation difference. Do not silently trim the corpus to force
the count. Distinguish an ore marker object's type, a deposit's goods/quantity, and what a later
mine can extract. INFO capacity or the visible marker alone proves no tonnage.

## O1 — derive placed-deposit quantity and the no-ore path

Trace scene placement 0x10008490 through the assigned and unassigned marker paths, their object
slots and AnnoWelt deposit update 0x1007c0d0. Resolve the relevant type-to-goods/quantity accessors,
the distinct id-1661 case and the initializer/units of scene scale 0x100433c4 if it is relied on.
Record bounds, neighborhood geometry, arithmetic widths/rounding, overwrite versus accumulation,
and random draws in actual call order. An unread callee's effects stay unknown.

Derive hand examples for a real positive deposit, no assigned ore, overlapping eligible markers,
the special type and the nearest decisive boundary. Separate exact bytes from inferred names.
Check the plausible wrong rule with a distinguishing example; do not merely duplicate prose in
two scripts. Preserve original unknown behavior rather than substituting a favorable map rule.

## O2 — derive the treasure spot scan

Read AnnoScene 0x10008cc0 through its real return and its caller. Identify candidate terrain/object
predicates, traversal order, fallback/no-site behavior, selection draw timing and facing/kind draws.
Separate candidate geometry from route reachability and player accessibility. Pin empty and one-site
cases and a boundary/order discriminator. Follow enough called predicates to justify the claimed
rule; record any remaining edge precisely rather than declaring every site legal by a proxy.

## O3 — check the corpus with a bounded read-only audit

Apply the established rules to the same documented template set. Produce private per-template
diagnostics and compact aggregate counts for evaluated, unsupported, no-site and mismatching cases.
Assert nonzero execution and account for every input exactly once. Hash the complete inputs and
raw output; do not publish extracted original tables, masks, layouts, listings or assets.

If quantities depend on scenario parameters, test explicit labelled scenarios/inputs rather than
inventing an intrinsic quantity per island. A data check is not execution of the original. Reuse
the predecessor parser only after verifying the fields used; cross-check a small hand-derived
sample directly from original bytes. No emulation/runtime claim unless actually performed.

Keep the bounded audit scripts in private cache with source hashes and reproducible commands.
Check empty/malformed inputs and a wrong expected result while a legal control passes; do not
let an empty scan acquire a success verdict. These are recorded audit checks, not new CTest or
engine coverage. No long C++ campaign is needed for this documentation-only packet.

## O4 — report and stop before implementation

Write a focused new `docs/re/annoscene-ore-2.md` linked to the pinned predecessor, preserving
the predecessor section and its evidence. Append the bottom REPORTS entry with verified
rules, addresses, corpus accounting, hashes, decisions/reasons, unread edges and NOT_RUN. At most
add a necessary narrow index link. Private audit scripts are not installed in the project.
Do not edit src/, state versions, goldens, data/maps, game/, workflow/coordinator, TASKS.md,
STATUS.md or another lane. No owner map or defaults decision follows from this research.

Freeze, run applicable cheap checks, ordinary-push and observe exact-head CI. Send READY FOR REVIEW
directly to the Mac orchestrator with full head, CI, changed paths, evidence and the smallest next
implementation boundary. Stop after this bounded read/audit; OQ-151 contains other work and must
not be closed wholesale. Any genuine policy decision goes to the owner through the orchestrator.
