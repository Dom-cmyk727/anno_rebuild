# KBEWOHN-1 — complete imported figure identities before animal gameplay

Wave 2.6, selected after DOCS-SIM-1 as the next bounded gameplay dependency.
RELEASED 2026-11-18 to EXECUTOR II MAC after independent DOCS-SIM-1
portability correction review; DOCS hosted CI remains separately pending.
EXECUTOR II MAC, one packet of about 3–4 hours. The orchestrator reviews; do not
start reviewer subagents. Ordinary-push the owned task branch at checkpoints.

## Outcome and boundaries

Known original BEWOHNTYPE names must retain their correct semantic number when
original figure definitions enter the simulation. Today the adapter recognizes
only a subset; other known kinds silently become 0. Complete that mapping so
later wildlife, hunting and other unit work can classify actual content. This
packet does not create animals, wire their ticks, implement combat, or claim
that recognizing a type implements its behavior.

Create task/KBEWOHN-1 from accepted SIM-LIMITS-1 head
`2f9e4c0bf84cf09998a43c7ff8098fa1522f3a4e` as released by mac-lane.md in this
commit. This independent base passed exact-head CI44298551123. The pending
DOCS-SIM candidate is not a source dependency and is not silently accepted. Preserve every prior branch and evidence. Do not merge
moving main or the board, rebase, force-push or touch another executor's branch.
No intake of WILDLIFE-1a is needed. Wildlife lifecycle, extension-tag allocation,
original load-jitter ordering and remaining movement uncertainties stay separate.

## Read first

Read CLAUDE.md, AGENTS.md, PLAN.md goals, TASKS.md owner rules, the current
mac-lane.md, testing/evidence-contract.md and testing/README.md, then:

- docs/re/annogame.md: sparse BEWOHNTYPE table at AnnoGame.dll 0x10024cb8 and
  name lookup 0x10013a20; matching original module and FigurenHeader.dat locally;
- docs/re/wildlife.md, only the relevant class/route boundaries, and the accepted
  wildlife helper review: classification is a dependency, not live wildlife;
- src/sim/src/types.cpp figure_type_from_entry, kBewohn, registry digest and adjoining
  PRODTYPE/ROUTETYPE mappings; include/anno/sim/types.hpp FigureType;
- all bewohntype consumers, especially founding.cpp, original_units.cpp,
  ship_cargo.cpp, freight.cpp, placement.cpp, mine_source.cpp, scenario_formats.cpp
  and script_runtime.cpp; do not infer that the additional names have no readers;
- types_test.cpp, figure_facts_test.cpp, existing registry/corpus checks,
  tools/figure_facts_ref.py if present, tools/pins.py and the current pin manifest,
  docs/sim/state.md sections 9.7–9.8 and ADR 0006;
- the DOCS-SIM inventory via git show at corrected pinned ref
  `1fab3ca9ec80cb69a119ea7201e4a7332bd1ddf0`. It is reference material only on this base; do not merge its
  pending tools or attempt to run files absent from the accepted source.

The lane handover references docs/UNITS.md, but it was absent from both the
source and origin/orch/board-next at preparation. Do not search the whole home,
assume its contents, or block on that stale link. Use the named primary table
and existing annogame evidence. Keep binary dumps and complete original record
listings in ignored cache, not public git.

## K0 — inventory and freeze the finite contract

Before implementation, record a small acceptance table. From the matching
original bytes independently establish the table extent, sparse holes, the
number/name relation, aliases if any and the actual lookup direction. Cross-check
which names occur in the installed FigurenHeader.dat. The independent dispatch review found 57 original names and 20 existing real
mappings plus one empty sentinel: 37 missing names, correcting the handover’s
36. Reconcile these counts independently rather than forcing them.

The dispatch reviewer read pointer range 0x10024cb8..0x10024fb8 in AnnoGame.dll
SHA256 ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc.
Re-read the lookup/accessor boundary and loader name resolution as well as the
table. Record exact module SHA256, address/range, one hand-derived missing-name example,
a sparse boundary and an already-supported control. Distinguish legitimate 0
(WUESTE is explicitly named at index 0), absent field, unknown spelling and a known name
currently falling through. Preserve existing unknown/absent-name behavior in
this packet unless it prevents correct mapping; a newly proposed rejection
policy goes to the orchestrator, not silently into the loader.

The dispatch review found that no added name enters 97..128 or equals 53, 145,
146 or 150. Confirm this; it bounds existing ship/crew/script/ore behavior.
Inventory every affected numeric equality/range consumer. Predict whether newly
recognized types enter any existing hull, founding, script, ship or ore branch.
List the expected registry digest/pin changes and which synthetic fixtures should
remain byte-identical. Do not claim an unchanged registry just because layout
and state version remain unchanged.

## K1 — complete the semantic mapping

Extend or factor the existing named mapping in the repository's established
style. Keep number assignment explicit where the original table is sparse;
never infer values from lexical sorting or vector position after removing holes.
Do not use figure asset ids as class identities. Preserve every previously
supported name/value and keep unknown/absent behavior as K0 states.

The finite original vocabulary is source-format semantics, not a fixed limit
on figure count, asset ids, player count or future mods. A synthetic figure with
a non-original asset id using a supported class must need no engine edit.
No general plugin registry, class dispatch redesign, new schema/base/timeline,
new rules flag or default behavior toggle is authorized by this data correction.

## K2 — prove identity and consumer behavior

Use a test-side expectation independently derived from the original table;
do not construct expected numbers by calling the production lookup or copying
its generated output. Test all recognized names, sparse boundaries, already
supported controls, unknown and absent names, empty spelling, UNUSED and WUESTE=0. Do not mistake unknown
UNUSED spelling for the evidenced named zero.
Through the public figure parser, prove that a previously missing named type
now gets its correct class and that a mod-style figure id uses the same rule.

Run a corpus check over every local FigurenHeader entry: each known named type
agrees with the independent table. Count inspected, known, legitimate-zero,
absent and unknown records separately. Nonzero alone is not correctness.
Perform a before/after per-figure field audit: only the evidenced bewohntype
values may change; nav, hull, ore facts and the existing class-range predicates
must remain equal for shipped mappings.
Exercise the concrete readers K0 found: at minimum preserve the existing ship,
scout, donkey and ore behavior; add a distinguishing check for any newly entered
range. Do not write broad new feature tests for behavior this packet does not add.

Legal controls first, then isolated faults: remove a newly added mapping;
assign a wrong nonzero number; shift values across a sparse hole. Each must fail
the intended value/consumer assertion, not a compiler error or earlier missing
file. Restore and rebuild cleanly before acceptance. Two distinct faults may
share a concise test; no mutation framework or exhaustive campaign is required.

## K3 — compatibility and affected fixtures

The registry digest hashes FigureType::bewohntype. Measure its before/after
change and show any old-recording mismatch is caught by existing identity checks.
Do not hide a changed original registry behind an old digest, edit log headers,
or call old original-data replay evidence unchanged.

No base field layout changes are expected. Preserve global/section/PRNG/wire
versions unless the change proves it needs a separately reviewed decision.
Affected fixture regeneration is authorized through tools/pins.py regen with
an explicit selection of the identified affected producers, followed by check.
Separate independently checked synthetic goldens from C++-recorded corpus pins;
retain the old failures and explain every changed fixture family. Do not
regenerate unrelated records, update expected outcomes merely to get green, or
rerun all long sailing/render campaigns without an actual dependency.

If the measured content change would require a broader compatibility mechanism,
stop that dependent part with exact evidence and a small proposed solution.
Continue independent tests/docs; do not invent a migration or broaden this
packet into save-system work.

## K4 — focused gates, report and stop

Run cheap preflight and negative controls, then canonical affected Debug and
ASan/UBSan selections, required reference contracts, lint and formatting.
Run the local real-file registry corpus; identify no-assets CI skips honestly.
Use source/binary identities and measured host capacity, keep admission enabled,
and leave other jobs, coordinators and the owner's keep-awake process alone.
Target warnings/hosted CI are required as available; Mac is not target-platform
or original-runtime acceptance. Push the task branch normally, then report the
frozen source while exact-head CI runs so review can overlap it.

Keep cache/kbewohn-1/CURRENT.md as a short current-state note with base, next step,
owned jobs and evidence paths. Read it after compaction; old reports do not change
your task. Commit a compact evidence note and bottom REPORTS.md entry, including
mapping counts, content identity impact, preserved failures, every changed pin
family, legal/broken controls, selected/executed/failure/skip counts and limits.
No TASKS.md, STATUS.md or mac-lane.md edits by the executor.

Send READY FOR REVIEW directly to Orchestrator II Mac, with full head/tree,
CI link and evidence. The owner authorizes this reply routing. Stop there;
the orchestrator independently reviews source, the primary evidence and tests,
requests only necessary focused corrections, then releases the next town-chain
packet. Beyond six hours, report a clean bounded checkpoint rather than starting
an unrelated repair. No main push, force push, default flip or live-animal claim.
