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

## I0–I3 reviewed checkpoint

I0 approved by `/root/integrate1_contract_review` at `c41ea1fe`; I1 by
`/root/integrate1_merge_review` at merge `a81d6c95` (parents `c41ea1fe`, supply
`67158af4`); I2 by `/root/integrate1_identity_review` at `392d3989` after fixing
an invalid oracle continuation and extending actual predecessor log coverage.
I2 canonical Debug:17/17 passes, zero other outcomes. Archive now has136 exact
input blobs, including retained reference goods tables, plus four exact native
save slices with separate provenance.

I3 approved by `/root/integrate1_joint_review` at `12424236`. Initial Debug and
ASAN gates each22/22; correction gates each13/13 (overlapping selections, not
35 unique cases). All zero other outcomes/sanitizer reports. Four targeted
mutants fail intended slot-hint, raw1, Pending barrier and repeated-end
assertions; source hashes restored, clean Debug rebuilt. Review corrected an
incorrect authored resource703 to independently verified GRAS2 -> growing1,
and the singular animal selector to plural, adding six actual animal cases.
The corrected complete native fixture proves positive crop and animal-line
consumption/production after load,120 animal-site draws over60000ticks and exact
continuation with the nested actors intact. The initial fixture/build/probe
failures remain under `cache/integrate1/`.

The original Tutorial prerequisite is **Allowed**, verified by public paid
Demolish/Build809 at261,344:250gold,512raw tools,1024raw wood. Its whole supported
empty kernel remains present and Ready. No authored permission override or
recovery substitute is required. These facts supersede historical supply notes
that the engine ignores scripts or lacks INFRAMIN; those notes remain attributed
input history. `house_updates` remains false by default.

## I4–I6 consolidated checkpoint

Both original-data achievements pass on the combined binary, with explicit
component projection/refusal controls. I5 broad Debug retains1513passes/4 failed
stale expectations; correction gate55/55 and affected ASAN716/716 pass with zero
other outcomes. Runtime-equivalent reuse is independently reviewed; the original
Debug fresh canary169.84s and ASAN587.38s produce identical recording artifacts.
Final I6 APPROVED documentation0e10a303 / runtime-testcafc1a7c; all I0–I6
packages are locally satisfied. Exact ledgers,
compatibility, source/workflow hashes, preserved failures and future canonical132
consumer delta are in the [consolidated handoff](../orchestration/reviews/2026-11-05-map-integrate1.md).

# MAP-INTEGRATE-2 (state 138)

Contract frozen before either authorized input merge, 2026-11-11. State 138 is
the union of automatic freight, salt extraction, iron/tools and quarry/bricks on
current main. It is not a new gameplay policy: all opt-in rules remain off by
default, and the state number distinguishes the combined byte layout and
behaviour from every input lineage.

## Frozen inputs and inventory

| Role | Exact commit | Base against current main | Published identity |
|---|---|---|---|
| Current main / branch parent | `03bf2b3cf6a7779e559c6dd3669ee24cccccb2ae` | — | 134 / ECONBK1 / PRNG 5 |
| Automatic freight | `49ffcee66f5bdb678e4e165112abacccd84e727a` | `e0bb6be7bd26dddd53d2f7e69ae29651fc5c49b9` | route-only 136 / ECONBK1 / PRNG 5 |
| Salt + iron/tools + quarry/bricks | `a5a3bca35c781231ab8d52205a5a1117b4cce23f` | `70d311cde090e4d34ccaa486861a58982fee31c8` | 137 / ECONBK1 / PRNG 5 |

The input ranges contain 96 Route paths and 183 Extract paths: 54 Route-only,
141 Extract-only and 42 shared. One-sided files remain in scope when a caller on
the other side changed. At the frozen main, merge-tree predicts three conflicts
for Route (`REPORTS.md`, `world.hpp`, `budget_f3_test.cpp`) and 23 for Extract:
the reports and open questions; `world.hpp`; cargo/hold goldens; current-state,
identity, replay and scenario tests; `budget_f3_test.cpp`; both independent
stream tools; and current `sim_replay` registrations. Route against Extract has
23 conflicts, centred on the same state/golden/test seams plus `apply.cpp`.
These counts are an inventory, not permission to resolve a subsystem wholesale.

## Interaction contract

| Seam | Route 136 responsibility | Extract 135/137 responsibility | State-138 resolution |
|---|---|---|---|
| Saved state | Append `FreightRules { max_stops, max_actions, work_per_visit }` after `Rules::city_booking`; append a figure's `u64 freight_generation` and optional 19-field `FreightState` after resident state. The route holds its stops/actions, status/phase, cursors, order/generation, timing credit, bounded hold scans, remaining transfer, cycles and failure. | A mine's optional deposit binding is already inside its `Mine` payload. Append `Quarry` payload tag 9: the `Inert` prefix and a bool claim per entrance. For mason job `0x2c` only, append `HarvestState::goal_col` and `goal_row` after `along`; all other jobs retain their bytes. | Preserve both append orders. Rules end with the three freight u32s. Building payload keeps alternatives 0–8 byte-identical and appends Quarry as 9. Figure order is base fields, optional nav, optional harvest (plus the two mason-only bytes), optional resident, freight generation, optional freight. Every member has one `fields()` line and is covered by the corresponding `field_count` assertion. |
| Commands | Append body alternatives 12/13 for assign/control and reserve wire tags F075/F076. Public Move/Stop/Load/Unload/Found takes over an assigned route after an accepted command. | Mine and quarry construction uses the existing `BuildCmd`, body alternative 0 and wire tag 0x64; eligibility/placement adds no command shape. | Keep command wire 1 and the complete body order through Load(11), Assign(12), Control(13). F075/F076 decode only their route bodies. The manual-takeover visitor visits `Command::Body`, not `Building::Payload`, so Quarry cannot reach it. Build retains 0x64 and gains no alias. |
| Building payload visitors | Freight identifies warehouses by type/cargo destination and never exhaustively visits `Building::Payload`; its only `std::visit` is the command-body takeover above. | Quarry is a real payload alternative even though the class table remains `Inert`; sim creation, validation, line switching, state and invariants must handle it. | Union every sim visitor. Main's app visitor is also a required caller: add a `DescribePayload(Quarry)` overload and a `payload_kind_name` Quarry arm, held by one formatter test. Do not weaken a catch-all or relabel Quarry as Inert at runtime. |
| Goods and cargo | A route validates `goods != 0`, the registry entry and warehouse stock bounds, then delegates transfers to the existing `ShipCargo` hold/city rules. It has no good-name allowlist. | Registry data adds salt, iron ore, iron, tools and bricks and the real chains that make/use them. Existing exact holds and cargo bounds remain authoritative. | All five goods can be route actions when present in the registry; capacity, same-good top-up, empty-hold selection, stock, ownership and conservation rules remain generic. Rebuild the current cargo and hold byte fixtures under 138; retain their payload cases exactly, changing only identity/rules suffix where appropriate. |
| Walks and scheduling | A navigating assigned ship is dispatched through `FreightRoutes::visit` in the existing figure slot and performs bounded route work; production-backed R5 counts were pinned on the 136 world. | T3-Q1 makes transport, harvest and resident walking share exact selected sub-cell endpoints; T3-Q2 replaces the resident reverse adaptation with the original-style forward home search and retains its failure branch. Mason job `0x2c` shares HarvestState but uses its own body. | Use the 137 shared walk implementation and the 136 freight dispatch branch. Re-run R5 on 138. A moved wood-cycle count is acceptable only when the ledger attributes it to the shared walk/scheduling semantics; never copy the 136 count into a changed trajectory. |
| Draws | Adds no draw site and consumes no draw: freight work is deterministic in figure/stop/action/hold order. R5/R6 observe the existing site counters and cursor but do not add a caller. | Salt, mine re-lookup, quarry/mason and the shared home walk add no draw site and consume no draw. Extract's real-data tests observe existing `Event750Island`, `FarmWorkerDispatch`, `HouseWalkerTarget` and `HouseVariant` counters; changed totals are behavioural timing effects. | PRNG timeline remains 5 and `DrawSite` is byte-for-byte unchanged. Its complete enum order is `SmallFarmAnimal`, `Event300`, `Event750Gate`, `Event750Island`, `CityRandomEvents`, `WildlifeSweep`, `Ai`, `LinkRawMaterial`, `LinkOre`, `LinkTemplatePick`, `LinkSpecialMark`, `LinkTreasure`, `LinkOrePlace`, `LinkNativeName`, `LinkTreasureKind`, `LinkTreasurePlace`, `ComputerName`, `CityName`, `CityNameLoad`, `FarmWorkerDispatch`, `HouseWalkerTarget`, `HouseVariant`, `HouseFirstAssignment`. Freight, mine, quarry and mason add no site. Compare per-site counts and cursor; explain trajectory-dependent totals rather than re-pin silently. |
| Rules and registry digests | Freight is the sole new Rules member and its three fields enter the Rules digest once through the common field list. | `BuildingType::highmap` and ore figure `ware` are tagged registry-digest additions; Quarry's runtime payload does not alter the class-table digest. No new Rules member. | One Rules suffix, one registry transcription. Preserve the conditional highmap and figure-ware bytes exactly once. State 138 changes the header even when defaults make freight inactive; digest mismatches retain their own diagnostics. |
| Goldens and pins | Owns `freight/current136`, Route state pins and current registrations; changes every `kStateHashVersion == 13x` expectation it touched. | Owns `salt1/current135` and `iron2/current137`; 137 changes scenario `producers_moved` and `warehouses_moved` via corrected type facts and movement, plus current registrations and state pins. | Preserve each input folder as historical/refusal evidence. Generate `current138`; move only current-behaviour `sim_replay` registrations to it. Recompute shared `cargo_rules_bytes.txt`, `ship_holds.txt` and `scenario_corpus.txt` from reviewed 138 behaviour, explaining every delta. Search the whole tree for 13x pins; no current134/135/136/137 path may masquerade as current. |
| Main since the bases | Route predates later main changes. | Extract also predates part of current main. | Main wins as the platform, including the `5e7b13c6` real-file repairs, portable CI fixes, and the five repaired `anno_app_tests`. Conflict resolution must port both features into these callers rather than restore an input's older copy. Orchestration-owned TASKS/STATUS/HISTORY/REPORTS-1 remain main's. |
| Open questions | Adds none. | Adds OQ-552, 553, 554, 555 and 556. | Current main ends this range at OQ-551 and intentionally has no 552–559 headings. Import the five numbered entries unchanged by number (including the source's 555-before-554 order if preserving text); do not renumber or invent 557–559. OQ-556 is answered separately in part R from original code, not guessed during merge. |

### Identity and compatibility

- The native stream is **138** with the existing `ECONBK1\0` schema, replay
  frame 4, command wire 1 and PRNG timeline 5. There is no new envelope field,
  script grammar or draw site; the native number changes because saved fields
  and combined behaviour change.
- Valid native states 134, 135, 136 and 137 all refuse with the native-version
  diagnostic. Each control uses a valid outer save and has a neighbouring valid
  populated 138 control. In particular, 135 now uses the retained Salt artifact;
  it is not simulated by editing a 136 header.
- Valid replay artifacts for 134, 135, 136 and 137 refuse with the replay's
  state-stream diagnostic. A current replay header embedding an earlier native
  state refuses at the embedded-native check. A wrong replay frame and a wrong
  command wire remain separate controls. Corrupt checksum/framing is never
  counted as a version refusal.
- Rules, registry, script program, schema and PRNG timeline mismatches remain
  distinct checks. A passing current header cannot cover them, and no 134–137
  artifact is rewritten and called historical evidence.

## Evidence plan

The independent Python serializers transcribe the combined order and exercise a
populated world containing: a route ship in flight with partial credit and hold
scan cursors; a Quarry with a nontrivial entrance claim set and an active mason
goal sub-cell; and a bound mine with two reservers. C++ round-trip/hash output is
compared with that transcription and is not itself the oracle. `current138`
goldens are generated only after the layout review and are accompanied by a
file/old/new/cause table.

Focused I2 evidence covers native state, replay, stream oracle, registry/rules
digests, command codec, current registrations and all four real predecessor
identities. I3 then proves the cross-feature authored fixture in Debug and ASan,
including new-goods freight, mine demolition, the three-state reload cut,
suspend/cancel and one replacement-verified mutant per seam. I4 re-runs affected
accepted game-file proofs (Route R5/R6, salt, iron T3–T5, quarry Q3, T3-Q2,
main's 18 real-file tests and the expansion/ship-start canaries). Game-file runs
are classified selected/executed/failed/empty/skipped, and a reviewer either
runs each accepted game-file test or records it NOT_RUN.

## Decisive acceptance cases

| Interaction | Legal control | Smallest counterexample |
|---|---|---|
| Combined field order | Populated 138 serializes identically in C++ and both independent Python transcriptions, then reloads byte-identically. | Omit freight generation, one Quarry claim, or mason goal byte: the oracle offset/hash and continuation differ. |
| Command union | Existing Build mine/quarry and F075/F076 round-trip to their distinct body alternatives. | Swap F075/F076 or decode either as Build: exact body/type assertion fails. |
| Payload visitors | Inspect a live Quarry and receive Quarry kind/status/claims; sim invariants accept its entrance-sized claim set. | Remove the app Quarry overload or sim case: formatter/visitor or invariant test fails rather than falling through as Inert. |
| New-goods freight | Automatically carry chain-made tools or bricks between owned warehouses for two cycles with a closed goods ledger. | Unknown/out-of-range goods, foreign warehouse, mismatched/full hold or insufficient stock is refused without mutation. |
| Shared walks | On 137's fixtures, transport/harvest/resident/mason finish at the selected sub-cell; a route-backed production run conserves goods. | Reintroduce tile-centre or reverse-home handling: endpoint/failure count or attributed R5 trajectory changes. |
| Draw order | Active freight + quarry + mine leaves the four-site table/order intact; recorded per-site counts and cursor reproduce across reload. | Add a draw in any new visit/step: first differing site/cursor is reported. |
| Digest union | Current Rules and registry load, and independently transcribed digests match. | Change one freight rule, highmap byte or ore-figure ware: the appropriate digest refuses for its own reason. |
| Historical/current goldens | Every current registration names `current138`; explicit historical tests consume unmodified 134–137 artifacts. | A current registration naming `current136`/`current137`, or a rewritten predecessor, fails the path/provenance audit. |
| Main preservation | Main's repaired real-file/app cases run on the union, including Quarry inspection. | Restore an input-era caller: the targeted main regression or app test fails. |
| Open-question numbering | Headings 552–556 appear once with their Extract text; Route adds none. | Duplicate, renumber or occupy 557–559: the heading audit fails. |
