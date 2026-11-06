# MAP-EXPAND-1: second-island settlement and manual resupply

## Scope and exact adoption

Assigned E0–E7 by `c5c7007f374a3ead95303150dea877b5590d45d6`, TASKS section
`MAP-EXPAND-1-MAIN`. Owned branch `task/MAP-EXPAND-1-main` starts at that exact
pin in the existing MAP-BOOTSTRAP-1 checkout. Fetch observed newer origin/main
12b5af43, but it is not the start or an integration input. Published gameplay
baseline303b3faf is an ancestor; its intervening changes to the dispatch pin are
only TASKS and gameplay-next-chain documentation. Preserved completed ECON branch
is467373d2; its ignored evidence and all prior build directories stay untouched.

Current identity remains state133 / ECONBK1 / PRNG5 / replay4 / command1 / save1.
No layout, rules default, registry, navigation trajectory or production change is
planned. New tests/recipes are not authority for an identity change. A demonstrated
production defect needs a causal reproducer, bounded review and an explicit
compatibility decision if saved layout or deterministic trajectories would change.
Do not silently patch published133. No main merge, push, active-map install,
loadout rebalance, new production class, automated route, foreign trading,
coordinator/runner repair, UI or first-Mac salt/cloth/hunting work is authorized.

The real arm uses unchanged lan5-ship-a, an actual owned ship and crew, two distinct
islands and public commands only. City booking is explicitly enabled, house
updates remain default-off. Peace, shared information and no alliances are
asserted; visibility is never an ownership grant. Finite inherited food/tools and
founding wood remain labelled. The result is freight, not settlers, profitability,
a balanced town, original-runtime ship collision fidelity or network multiplayer.

## Acceptance and dependency table (before implementation)

| Package / requirement | Positive proof | Distinguishing negative / legal control | Evidence / independent reviewer |
|---|---|---|---|
|E0 adoption and plan|clean exact successor, preserved ECON/evidence, host/admission/tools and cheap canonical contracts|moving main not adopted; first PATH failure retained; completed preflight not counted as voyage|E0/adoption.json,host.json, canonical-debug-01.log; expand_e0_review PENDING|
|E1 physically feasible itinerary and budget|same owner can found on distinct islands; actual route endpoints and cargo docks; reachable forester/road; complete input hashes and finite bill|geometry witness alone cannot pass; unsupported stage/source/road/dock refusal recorded; disposable planning cannot mutate pristine real world|bounded planning diagnostics, raw HEAD/LOGIC derivation and budget; review PENDING|
|E2 source produces export|zero initial/imported source wood, real cuts/raw/output/cart receipt, exact LoadCmd debit=credit and residual|public switch off immediately after construction before any advance prevents workers/production; same source request is StockShort|per-home raw/output/in-flight/store ledger, unfaulted separate native fork; review PENDING|
|E3 second founding and construction|same actual ship arrives, second island/city/owner/footprint/height; exact founding and unload costs; paid receiving building|with declared tools already transferred but wood missing, BuildCmd refuses unchanged; same command succeeds after freight unload|public outcomes, per-city/per-hold stock and gold snapshots; review PENDING|
|E4 repeat and adversarial ownership|two completed source-load/destination-unload cycles with further production; independent homes/stocks|wrong owner, absent/demolished target, out-of-range, shortage, holds full/mismatch, partial/repeat and closer rival explicit-ID controls|short labelled synthetic companions plus real repeat; no transport cache privilege; review PENDING|
|E5 economy/native/replay|all owners' money decomposes exactly, each city's independent stamp/cost/bill,5 native cuts, genuine fresh-process replay|accepted legal changed transfer diverges after matching prior checkpoints; isolated wrong-debit/credit-only/repeat/stale-target mutants fail intended assertions while controls execute|reuse accepted bac3 simultaneous city/player upkeep; own actual itinerary and fresh CLI; review PENDING|
|E6 reusable consumer recipe and bounded cost|typed public commands, exact selected targets/ranges/results; several legal sizes, operation counts and separate timing samples; alternate supported data|no full-world cargo scan introduced; different goods/warehouse/owner fixtures; bounds/refusals retained rather than force success|headless recipe Windows can consume; no window code; review PENDING|
|E7 frozen closeout|relevant Debug and ASan/UBSan, at least1 current-source combined multi-island sanitizer voyage, exact selection/contracts, fresh independent integration review|first failures remain failures; missing/empty/skip/incomplete/sanitizer and malformed evidence cannot become a pass|one reviewed handoff+REPORTS, exact receipts/hashes, clean committed local candidate and no owned jobs; fresh reviewer PENDING|

Each approval releases dependent work without routine orchestrator pauses. E1
must establish starting-state, cargo, docking and material prerequisites before
expensive itinerary gates. A real blocker is recorded with examined bounds and
first authoritative refusal; independent short contracts continue. No partial
search establishes global impossibility.

## Existing contracts to compose, with explicit new risks

- `MoveCmd` acceptance is not arrival. Match ship and order_id in ShipArrived;
  route failures are outcomes. Keep existing rational navigation, bounded search,
  goal acceptance, density-only planning and omitted local collision/replan limits.
- Existing two-seat opening is not same-owner two-island proof. Reject source
  island for destination selection; FoundCmd already refuses another own city on
  the same island. Assert concrete CityRef, warehouse ID, owner and terrain height.
- Founding range does not imply cargo range. Validate the footprint digital disc
  AND city-owned outline against the actual founded planning world; a named
  warehouse is re-resolved at apply. Nearest rival geometry grants no access.
- Existing forester canary unloads38t inherited wood after founding. Reuse its
  mechanisms/site hints, not its all-unload sequence. Source warehouse must stay
  at0wood through public founding/construction and tool-only unloading. Forester
  and dirt roads cost0wood. Planning copies detach diagnostic sinks and never
  publish relocated coordinates/resources into real execution or its initial save.
- Load source stock rounds DOWN to whole tons; chosen hold room may still make
  the actual transfer fractional. With room>=256, stock448 permits256 and retains
  192; stock255 is StockShort. Record actual transfer/hold resolution, not request.
  At first export drain the supported whole-ton available stock (bounded by the
  inspected hold room), leaving<256. A second source load then requires further
  production, not reuse of an earlier whole-ton balance. Record per-home new
  production after first load; no quantity is seeded to force the448 example.
- No-production control starts at the unfaulted pre-production cut: raw/out,
  worker count and in-flight goods are0; public switch off occurs before any
  advance. Merely switching off after workers carry cargo is insufficient.
- Complete goods attribution is a transfer/conservation ledger plus the zero-
  source control. Fungible holds gain no invented provenance state. Original
  founding wood and tools remain finite inputs, even if they share a hold with
  the exported new wood.

## Preliminary independent bill (not a feasible-voyage verdict)

Original assets are read-only. Independent E0 reviewer decodes raw chunk records,
not the C++ registry as its expected answer. Anlagen SHA256
b2317a5c24b8c9fe796cf40f3fedaefcfa30ad3f185df9e69b2dca6a03d8dd75;
original AnnoWelt SHA256f220a878…6ae2 as in accepted primary notes.

| Item | Gold | Wood tons | Tools tons | Off / active city cost |
|---|---:|---:|---:|---:|
|1302 founding, each of2|350|12|5|0 /15|
|801 source forester|150|0|2|4 /12|
|2520 dirt road, each R|5|0|0|0 /0|
|1310 possible receiving stall|50|1|1|0 /5|
|Total proposed construction/founding|900+5R|25 (24 aboard founding,1 receiving)|13|source27,destination20|

The base ship's original50wood/80tools/10food tons and30000gold remain unchanged;
4 cargo holds, each50t, and hold_count_b2 is not cargo addressability. Tools hold
entries remain distinct. The receiving1wood can be supplied by new freight;
preliminary total costs do not assert that every construction material is new.
Before active building times are measured, upkeep is not a final number. At each
booking, source27/15=1 and destination20/15=1, independently; incorrect merged
47/15=3 is a distinguishing counterexample. Player upkeep buckets/remainder are
observed separately even if all are0. Reuse the accepted nonzero simultaneous
city28/player14+remainder1 control at bac3eb37 instead of reopening ECON.

E1 inventories complete manifest bytes/content hash, base scenario, all referenced
templates, Anlagen, FigurenHeader, BGruppen and city-name inputs actually read by
the loaders. Expected manifest SHA25620d948c97b1801946d71dfa86762e9b0fea5e6cd093314d83cb5b43658b2fd20,
content hashe001442a55b8ee0890ec5cd3d23e5a53f05d66fe3a29fdb1c1b47d19a3363d92.
No input or resource is altered to close the bill.

## Search, pilot, gate and host policy

Planning caps are explicit fixture/operator work limits, not engine limits. Begin
with known accepted first-island/forester hints, revalidate all public commands
against a fresh untouched start, then examine at most8 legal destination-site
candidates across other unchanged manifest islands and at most64 water-dock
candidates per site; record every attempted route/refusal and exact limits. The
initial source search is one accepted first-coast/forester hint only (801 origin
238,128); no fallback forest search is authorized by that attempt. Bound its
production trial to200000ticks /400 advances at1000ms, and each sailed leg to2000
advances at1000ms (1000000ticks); every visit age stays500ticks. A source/site or
arrival failure stops that bounded attempt and retains its first refusal/elapsed
work. Review any expansion of these operator fixture caps before running it;
these guards neither alter the engine's limits nor establish impossibility.
If the bounded search is insufficient, retain results and review a justified
expansion, never silently replace a map or treat a witness as navigability.
A forest site is not accepted until its legal road and actual delivery execute
on a disposable world. Advance elapsed1000ms (500ticks) is a proposed measured
pilot setting inside the existing supported range, not a rule change. Validate
arrival/no position jump, first fractional receipt and bounds in that pilot.
Use identical partition for uninterrupted/native/replay arms, without claiming
world-wide advance-partition invariance. Keep stamp ages far below65536 per visit.

Host sample E0: Mac17,6 arm64,18 logical CPUs,48GiB RAM,~1.53TiB free, no other
build/test observed; no ANNO_TEST admission overrides. Existing4 build/4 CTest
slots remain bounded and admission enabled; serialize long own lanes, respect
existing heavy labels and use pilot-derived per-case deadlines. No PID stopping
is needed; never pattern-kill or touch another executor's jobs. Old Debug/ASAN/
econ builds are preserved. Use new `build/expand-debug`, `expand-asan` (and modest
Release measurement if needed), with explicit own source/build roots and targets.
Reuse the reviewed headless bench_picture-only unused-argument warning exception
via ignored local preload; no sim/formats/load/replay warning exception or build
workflow repair is introduced.

E0 preflight first shell lacked the known tool PATH: retained initial logs show
missing cmake, not test failures. With the existing mac-tools PATH, compiler-free
preflight25 contracts and workflow21 methods pass. Canonical E0 at clean c5:
selected3/executed passes3, failed/empty/skipped/incomplete/sanitizer lines0;
6 required invocations separately1/1. Exact selection: cargo_reference_agrees,
bootstrap_budget_original, integration_stream_reference_agrees. Original assets
required, admission defaults enabled. Raw `cache/expand1/E0/canonical-debug-01.log`
SHA25605f27066f206e42ae7744e7fbb8f3ff9733402d990cd234b87c1a5408095f3ac.
No original long opening or closed ECON matrix was repeated for E0.

Current disposition: E0 plan REVIEW PENDING. E1–E7 are not yet implemented or
accepted; the preliminary arithmetic is not a physical multi-island proof.
