# Gameplay integration: MAP-INTEGRATE-1

Contract frozen before the local merge, 2026-11-05. This is bounded headless
integration of accepted supply, scripting and original actors, not completion
of M3 or a balanced ship-founded town. The dispatch is main's
`cafa74d51ee6b03d514cf197a4e5b1c80ab2219e`, read from Git objects; the local
historical TASKS board is not replaced.

## Inputs and ownership

| Role | Exact commit | Identity |
|---|---|---|
| Common ancestor | a50aec3a6b0f6bbeb90fee5525c757f2b58b4da0 | published 128 / timeline 5 |
| Accepted UNIT1/SCRIPT2, starting parent | 50aa32e38706ad3c2e4302e8fcaada9f555ba034 | 130 / SCRIPT2 schema / timeline 5 |
| Accepted SUPPLY1/CLOSE, sole authorized merge input | 67158af4d29d87fed8686dc6542ca2a9f4b1f75c | supply 129 / timeline 5 |

The owned checkout was clean at UNIT1. `task/MAP-INTEGRATE-1` was created from
that exact head; UNIT1 and SCRIPT1 refs remain unchanged. One local no-ff merge
of the supply hash is authorized after I0 review. Main and active NEEDS2 are
excluded. No push, rebase, other executor edits/builds or process termination.

The machine-readable I0 inventory is `cache/integrate1/I0/input-inventory.json`:
151 UNIT-side paths, 53 supply-side paths, 17 common paths, with SHA-256 of
available input blobs. One-sided paths are also part of caller review: supply's
`driver.cpp`/`harvest.cpp`, the UNIT original-import/scenario/session/replay
callers, both test helpers and the stream reference/fixtures. Input evidence is
historical and must remain byte-identical, not relabelled as an integrated run.

## Semantic resolutions and responsibilities

| Interaction | Accepted responsibilities retained | Integration obligation |
|---|---|---|
| Normal scenario creation and INIT | UNIT strict import creates required actors before INIT; unsupported actors refuse. Component actor-free choice is explicit before loading. | No fallback to an actor-free world after failure; retain generated loadout path separately. |
| Figure registry and supply dispatch | UNIT source slots are independent of serial FigureIds; allocator hint/cursor, MAIN capacity and next ID survive. Supply dispatch uses normal add_figure and raises home counters once. | A farm-created worker must allocate a new ID and next free slot without overwriting imported ship/crew metadata. |
| Harvest body and validation | Supply extends accepted Farm job 0x29/0x2a to SmallFarm and animal 0x2e, with raw input selected by configured goods. UNIT adds cargo, parent and navigation checks. | Keep both validators and all ordered state fields; no Farm-only regression or universal raw-slot-zero shortcut. |
| Termination and claims | Shared end finishes active harvest and releases home once. Faithful claims retain their documented bit-only behavior; enhanced claims release only their owner. | Public repeated end/demolition controls must not debit twice or erase another worker's claim. No default rule changes. |
| Construction and production | Pending script startup is a construction barrier; Ready restore does not re-run INIT. Permission checks precede an otherwise valid command. Production uses configured types/roles/goods. | Missing context is never unrestricted; permission allowance must still enforce ownership, terrain and cost. Different-owner control required. |
| Population and diagnostics | house_updates defaults false. Supply diagnostics are sinks, not authoritative state. | Real supply tests opt in; no sink-dependent world evolution or native fields added for ledgers. |
| Registrations and documentation | Both input test families and narrow lint exceptions remain. | Union registrations; resolve docs by subject, preserve attributed evidence and original failures. TASKS remains orchestrator-owned. |
| Identity and references | Historical numeric 129 refers to two distinct lineages. UNIT's replay frame3 lacks a native identity check at the public API. | Fresh combined identity and explicit replay checks; archive each predecessor independently before changing current references. |

No change is authorized to script handlers, actor support classes, land movement,
combat, active map selection, economy balancing, UI/audio/net or default House
updates. Accepted tile-level routing and original-versus-enhanced adaptations
remain as documented in forestry and small-farm-supply contracts.

## Identity decision (I2 implementation contract)

- Combined native stream **131**, with lineage schema tag `SIMINT1\0`
  (`0x0031544e494d4953` in the existing schema field). The schema name identifies
  the combined contract, not a claim that script grammar changed. Script policy
  remains 2. Save outer frame remains 1; command wire remains 1: no command or
  save envelope field is being changed.
- Replay frame **4** carries explicit native stream and lineage schema fields.
  Readers refuse old frames 2/3 rather than interpreting them as current. Public
  replay execution checks the identity even for directly constructed logs with
  no checkpoints. A claimed current header does not authorize an incompatible
  initial native save. Preserve segment/suffix replay: identity validation does
  not require the supplied live world to equal the recording's initial world.
- Timeline **5** remains: neither input adds a draw-site enum, phase, ported
  classification or missing-site change relative to the common ancestor. Supply
  enables existing named dispatch/animal sites; script/import add no PRNG site.
  State evolution changes are distinguished by native identity. Joint runs must
  still compare per-site counts and PRNG cursor across native continuation.
- Config, Rules, registry, walk, seams and script-program digests retain their
  own roles and mismatch reasons. Fresh identity does not substitute for them.
  Later independently numbered NEEDS2 streams are not compatible by number.
- Preserve published128, supply129, SCRIPT1-schema129 and UNIT130 separately,
  including source provenance and fixture hashes. SCRIPT1/2's independent
  references currently import the published128 reference: retain that dependency
  explicitly when installing the current combined oracle. Never collapse both
  129 histories into a single generic predecessor.
- Refusal controls must retain valid outer framing/checksum. Include current
  legal controls and a direct checkpoint-free predecessor ReplayLog control.
  Rules/registry/program mismatch controls otherwise carry current identity.
- Independently transcribe combined field order and digests. The decisive oracle
  fixture contains nontrivial script signals/permissions/continuation, actor
  slots/parents/nav/holds and HarvestState including animal acceptance. A
  C++-generated hash alone is only a rebaseline, not that oracle.

## Small proofs and acceptance table

| Package/seam | Legal control | Smallest distinguishing counterexample / acceptance |
|---|---|---|
| I0 Tutorial farm809 prerequisite | Original Tutorial component load retains complete supported empty kernel, initializes normally; affordable public rebuild809 at its freed original footprint. | Raw player0 flags0x3f include INFRA_EVER bit0; no809 override was found. This predicts Allowed, not denial. Verify the public command cheaply before resilience runs. If actual original permission denies, preserve refusal and explicitly author a separate permitted recovery fixture. |
| I1 import + dispatch | Explicit source slots such as ship37, crew2, donkey7 with MAIN100: serials1..3, hint7, nextID4; a real Advance dispatch creates ID4 in slot8. | Losing hint, conflating raw slot with serial ID, or overwriting an imported slot must fail exact slot/ID/counter checks. Do not mix explicit and automatic FigureSpecs. |
| I2 native identity | Current populated save loads and continuation agrees. | Resealed128, both129 lineages and130 refuse at native identity; wrong schema at131 refuses separately. Retained actual predecessor artifacts and provenance complement header controls. |
| I2 replay identity | Current encoded log and direct current log execute, including no-checkpoint control. | Old frame and direct predecessor object refuse before execution; forged current header with old initial save cannot bypass. Own-reason live digest controls still reach their checks. |
| I3 startup/permission | Authored supported script, original-data types/resources honestly labelled, public session INIT exactly once; later allow permits legal affordable build. | Pending/deny leaves world and stocks unchanged; different owner, bad terrain and unaffordable command remain refused even when permission allows. |
| I3 joint lifecycle | Parented ship/crew plus real farm dispatch/work/return while script visits run. | Check ID/slot metadata, one counter raise/release, credit/raw slot, claim policy and PRNG site counts; repeated public end has no second release. Reuse targeted existing wrong-reading controls where suitable. |
| I3 persistence | Save active worker plus Ready nontrivial script and nested ship/crew/cargo, continue against uninterrupted world. | Exact final native bytes, events, transfers and PRNG match; separate Pending cut proves INIT once. No loaded-versus-loaded-only comparison. |
| I4 unchanged endless01 | Whole original program, actors/resources/positions; real area wave, sail/found/unload/build through public commands; native cuts and fresh-process replay. | Denied-copy, legal changed command and valid changed program remain distinct. No source-world grants or relocation. Exact ledgers explain new131 hashes. |
| I4 supply | Crop/alcohol, animal food, household purchase benefit, interruption/recovery on combined binary. | Target town's four farms only; both initial stores256, live no-producer control buys >0 and produces no target farm food. Preserve explicit component projection plus separate normal-loader refusal. Old1183/756/237 are evidence, never forced inputs. |
| I5/I6 | Clean frozen source, exact canonical required contracts, broad Debug plus affected sanitizer, independent refs/format/lint, read-only final review. | Nonempty executed selection; no timeout-as-pass, hidden skips, stale binary or unrelated full-suite claim. Preserve first failures. |

I3 is an explicitly authored joint mechanics fixture, not unchanged original
world evidence. I4 supplies two separately scoped original-data achievements;
together they do not establish a complete earned ship-start town. OQ-491,
ship-founded alcohol feasibility and new cloth/hunting remain open.

## Evidence, scheduling and stopping boundary

Use current main's canonical `tools/test/run_selected.py` with explicit owned
source/build paths; record actual runner revision separately from tested code.
Admission stays enabled. Record host CPU/RAM/disk/contention before builds;
serialize expensive owned lanes and never stop another executor's jobs. Initial
host observation: 48GiB RAM, 18 logical CPUs, ~1.5TiB free disk, other-lane ASAN
processes active. Refresh before scheduling, do not infer host capacity from a
stale snapshot. Original assets remain read-only in main's `game/app`.

I0 contract approval releases I1. Independent diff/caller review releases I2;
identity review and short Debug/sanitizer I3 approval precede long I4/I5 runs.
Use new current-identity artifacts once, including long canaries in selected
runs. Reuse accepted UNIT1/SCRIPT/SUPPLY baseline evidence with exact provenance.
Do not rerun isolated long generated opening or heavy render solely for131;
new affected interactions require an explained timeout before the run.

Every gate records selected/executed/failed/empty/skipped/incomplete and sanitizer
outcomes, input/source/build/executable identities and artifact hashes. Mutants
are restored and clean binaries rebuilt before final gates. A code correction
reruns affected checks; prose alone does not invalidate tested runtime evidence.
Reviewers read frozen diffs, callers and raw evidence independently; findings
are fixed and re-reviewed. Keep reviewer IDs/pins in the final review ledger.

Finish with one consolidated REPORTS entry and
`docs/orchestration/reviews/2026-11-05-map-integrate1.md`, separating runtime,
gate, reporter and documentation identities, all first failures and NOT_RUN
Windows/Linux/live-original/render/network checks. Describe future Windows and
accepted NEEDS2 reconciliation from supply67158af4 without importing its active
work. Stop at a reviewed passing combined local candidate, or report PARTIAL
with a precise unresolved contract dependency while finishing independent work.
