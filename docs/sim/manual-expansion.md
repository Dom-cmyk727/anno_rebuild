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
No layout, rules default, registry, navigation trajectory or production change has
landed. New tests/recipes are not authority for an identity change. A demonstrated
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
|E0 adoption and plan|clean exact successor, preserved ECON/evidence, host/admission/tools and cheap canonical contracts|moving main not adopted; first PATH failure retained; completed preflight not counted as voyage|E0/adoption.json,host.json, canonical-debug-01.log; expand_e0_review APPROVEDbe8d163b|
|E1 physically feasible itinerary and budget|same owner can found on distinct islands; actual route endpoints and cargo docks; reachable forester/road; complete input hashes and finite bill|geometry witness alone cannot pass; unsupported stage/source/road/dock refusal recorded; disposable planning cannot mutate pristine real world|E1 canonical04 and raw bill/input receipts; expand_e0_review APPROVEDfcc71179|
|E2 source produces export|zero initial/imported source wood, real cuts/raw/output/cart receipt, exact LoadCmd debit=credit and residual|public switch off immediately after construction before any advance prevents workers/production; same source request is StockShort|genuine02 FAIL435f12d1 missing founding15; NOT APPROVED; causalrepro APPROVEDe1369d7a|
|E3 second founding and construction|same actual ship arrives, second island/city/owner/footprint/height; exact founding and unload costs; paid receiving building|with declared tools already transferred but wood missing, BuildCmd refuses unchanged; same command succeeds after freight unload|genuine E3 NOT_RUN; only E1 copied planning proof|
|E4 repeat and adversarial ownership|two completed source-load/destination-unload cycles with further production; independent homes/stocks|wrong owner, absent/demolished target, out-of-range, shortage, holds full/mismatch, partial/repeat and closer rival explicit-ID controls|bounded companions APPROVED637da167; real repeat NOT_RUN, fullE4 OPEN|
|E5 economy/native/replay|all owners' money decomposes exactly, each city's independent stamp/cost/bill,5 native cuts, genuine fresh-process replay|accepted legal changed transfer diverges after matching prior checkpoints; isolated wrong-debit/credit-only/repeat/stale-target mutants fail intended assertions while controls execute|accepted bac3 control reusable; genuine two-city/native/replay E5 NOT_RUN|
|E6 reusable consumer recipe and bounded cost|typed public commands, exact selected targets/ranges/results; several legal sizes, operation counts and separate timing samples; alternate supported data|no full-world cargo scan introduced; different goods/warehouse/owner fixtures; bounds/refusals retained rather than force success|bounded API/scale/consumer subset APPROVED637da167; full executed recipe OPEN|
|E7 frozen closeout|relevant Debug and ASan/UBSan, at least1 current-source combined multi-island sanitizer voyage, exact selection/contracts, fresh independent integration review|first failures remain failures; missing/empty/skip/incomplete/sanitizer and malformed evidence cannot become a pass|short ASAN16/16 only; combined sailing NOT_RUN; fresh bounded review pending; fullE7 OPEN|

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

E0 disposition: `/root/expand_e0_review` APPROVED
be8d163b3b9935cd283c916366b8ae2f95e19a37 after the focused source/production/arrival
cap correction. No runtime change or additional gate was needed for that docs-only
correction. Read-only reviewer independently decoded original cost records,
1502 hold facts and the stock-floor AND0xffffff00 at AnnoWelt100ac5bf, including
448→256+192 and255→StockShort. Notes `/private/tmp/map-expand1-e0-review.md`.
E1–E7 still require their own physical/evidence acceptance.

## E1 implementation and predeclared focused gate

The new `expansion E1: unchanged candidate route docks forest and finite budget`
case composes existing test fixtures without changing production. It plans on
copies, with source-world hash unchanged. It validates exact engine route ends,
FoundCmd and both cargo range predicates, the one known forest hint and an actual
road-connected fractional warehouse receipt. It loads all available whole tons
within real hold room, checks the remaining fraction, then plans distinct-island
founding/receiving-tool transfer and the before/after-wood BuildCmd consequence.
A return route is checked against the post-construction world. These are bounded
planning proofs, not a recorded genuine voyage or completion of E2–E5.

A complete input fingerprint superset under ignored `E1/content-inputs.json`
covers472 original inputs: original registry/walk/city-name/base files plus every
file beneath Scenes/Islands, including all scanner candidates. Manifest world
edge is1431×1431 (not the base scenario's1024 edge); original loadout/content hash
remain unchanged. Source bytes are not copied or modified. Exact per-trial
refusals and route work are retained in canonical raw output.

Predeclared E1 Debug timeout900s includes the existing admission margin; bounded
copy/planning+400 production advances is smaller than the accepted same-map
single-forest115.63s Debug workload. Measure this pilot before selecting a deadline
for the later combined sanitizer itinerary. Frozen focused selector is the one
new E1 case; required cargo/reference, bootstrap-budget and populated-state
contracts remain separate. E1 execution and independent review PENDING.

### E1 first pilot and focused review corrections

First canonical E1 build at4740aaf9 failed before selection: an unparenthesized
ternary inside doctest's streamed diagnostic. Retained `canonical-debug-01.log`,
status2/CTestnull, no selected test executed. Test-only cbc0ce7c corrects it.
Canonical02 at clean cbc0ce7c passes1/1 (239 assertions),6 exact prerequisites
separately1/1, all other outcome categories0. Raw SHA256
b5d7a25e1463c2a05796f5a49ac58ec549046f98a854a4def1083ef40ad1b2b6.
Planning15.25s includes original registry/load; its inner planning is6400.51ms.
Observed: source island0 click255,138 dock257,134; forester238,128,20roads and65
cuttable trees;37500ticks gives448warehouse wood,1tload/192residue. Destination
island1 click456,353 dock452,349 and receiver450,355 pass authoritative commands.

Independent E1 reviewer confirms the pilot, all472 input hashes (281337946bytes)
and the original primary facts. It requests three focused closures: repeat
outbound route after receiving construction, explicit travel/upkeep budget, and
another source batch within its declared production cap. These are addressed
before E1 approval, not waived on the strength of the initial PASS.

The strengthened second-batch cut counts existing HOLZ after first export as
P0=source residual+Farm.out+active cart cargo whose home is the source warehouse.
Reserved is already inside output; BAUM raw is not wood; ended/Done cargo is
history and excluded. Second request=floor(P0/256)+1 tons, with actual hold room
checked after second founding and first unload. Therefore its raw quantity>P0
cannot be supplied entirely by already-produced wood. The bounded second pilot
must observe enough new per-home made_88 and warehouse stock, load it with exact
debit/credit and re-plan the repeat source→destination leg on the resulting
post-construction/post-production world. This is still planning-copy evidence.
Both production trials use at most400×1000ms each; at most8 destination trials
imply at most400+8×400 planning production advances overall. No search cap or
production rule is silently changed.

Conservative main-itinerary gold feasibility, before actual timing:4 sailed legs
at<=1000000ticks each plus2 production waits at<=200000ticks each gives4400000
elapsed ticks. At most2 cities, each costing1 per2000ticks, plus at most1 first-
booking allowance per city, gives at most4402city upkeep. Player buckets start0
and these city-only lifecycle operations do not add player upkeep (the accepted
bac3 regression stays the nonzero simultaneous control). The generated start is
script-free and this plan builds no households. Construction900+5R with R20 gives
1000; total conservative spending<=5402 leaves>=24598of30000gold. Actual money,
player remainders and any other legitimate effects still must be reconciled in
E5, independently per owner; the bound is not a measured economic ledger.

### E1 final approval and release

`/root/expand_e0_review` APPROVED
fcc7117919d2f9d119dda1a30cb591849d911b83 after the focused corrections. Canonical04
passes1/1 with339assertions,6 prerequisite invocations each1/1, all other outcome
categories0. Selected18.68s; inner planning9492.91ms. Raw SHA256
652aa0d5d9f8e288c362cdb498b50f4db2c01f5ddcdda8868b819ec2a54c8cf8.
All5 receipt hashes and472 content hashes independently match. Both compile-only
failures (01 diagnostic parentheses,03 concrete transport-state access) remain
failed builds with no selected execution, not gameplay failures or successes.

Observed pipeline P0=374, second target512, minimum new wood needed138; additional
production490 gives warehouse654 in22000ticks, exact512load/residual142. Repeat
post-construction/post-production route reaches destination452,349. E1 is a
bounded copy-based prerequisite proof. The genuine first voyage advances the
world before founding, so E2 revalidates source/road, actual trees and quantities;
it does not inherit planning's37500tick/448receipt assumptions.

E2 will run one recorded genuine prefix (untouched ship start→actual arrival→
public founding/tool-only unload/forester/roads→real delivery/load), plus an
unfaulted separate pre-production native fork switched off before any advance.
All freight quantities are derived from the actual first receipt. The main run
never relocates actors or writes stocks/permissions/stages. Measured E1 work and
prior same-map sailing calibrate a predeclared900s Debug pilot deadline. Later
native/full-replay and second-island stages extend this same public-command
recipe; E2 is not full E3–E7 acceptance.

### E2 frozen source and focused pre-execution corrections

E2 source c6c273c2's first gate failed at compile before any selected execution:
signed aggregate-hold helper plus unsigned transferred amount under Werror. A
bounded explicit i64 conversion fixes the test arithmetic. The independent E2
reviewer also requested full ordered hold assertions and explicit accounting for
the public-off control's legitimate history change. Full holds now retain original
food and duplicate tool order, with second-founding/receiver reserves checked.
The control switch leaves money unchanged and changes city civil27→19 (15+4),
military0, without a player-cost change. Both floor27/15 and19/15 charge1 per
booking; every control step reconciles all owners, zero player buckets/remainder,
booking amount/order and the resulting equal cash despite different cost history.
The first build failure is retained; no gameplay result is claimed from it.

### Genuine E2 failure: founding drops prepared financial history

Canonical E2-02 at435f12d1 is FAIL1selected/0passes/1failed, with no empty, skips,
incomplete or sanitizer lines. Actual sailing reaches dock257,134 in86000ticks.
7120 assertions pass before the pre-production native control observes civil12
instead of27. No expectation is weakened: original1302 Market is constant-active
and running_cost[1]=15, while801 adds12. The real defect is the founding
transaction: create_building prepares the warehouse/index AND adds15 to temporary
local.accounts; CityCreation::commit makes the committed city with0accounts;
Founding::apply adopts local.index only, dropping local.accounts. Later forest
build contributes12, exposing the missing15. Native load preserves this already
wrong authoritative history; it must not silently rebuild/repair it.

The short `expansion founding accounting: public founding retains prepared
warehouse history` case isolates the publicFound call with originaldata and a
clearly labelled relocated fixture, no genuine-voyage claim. Default-off is its
legal control; enabled founding should retain15 and charge1 at2000ticks. The
proposed one-line runtime correction is retained ONLY as ignored
`E2/proposed-founding-history.patch`: adopt local.accounts beside local.index.
It reuses validated prepared history without scanning/recomputing, preserves
registration/name/PRNG/payment and default-off trajectories, but changes booking-
enabled founded states/recordings. No runtime fix is landed yet.

Per the packet, an explicit compatibility disposition coordinated with the
orchestrator/firstMac is required before landing this deterministic correction.
Do not silently amend published133, guess a shared next number, or disguise the
missing15 as an inactive warehouse. E2 remains unapproved; E3/E5 genuine accounting
acceptance depends on the correction. Independent cargo/isolation/scale work can
continue while the compatibility decision is obtained.

### Independent work while the founding compatibility decision is pending

The isolated original-data publicFound reproducer at clean e1369d7a is a terminal
FAIL:1 selected/0 executed passes/1 failed,56 of60 assertions pass; no empty,
skips, incomplete or sanitizer diagnostics. The default-off legal control retains
0accounts and29650gold. Enabled133 actually retains0 instead of15 and remains
29650 instead of29649 after its first booking. Both actual native snapshots are
preserved without repair in E2/founding-repro-01. The six separately required
contracts each pass1/1. Receipts canonical-debug-02-receipt.json and
canonical-founding-repro-01-receipt.json pin the raw failures/artifacts; neither
is superseded by a runtime correction yet.

The short `expansion cargo boundary: closer rival never overrides named owner
and stale target` fixture is deliberately synthetic. Ship10,10, rival footprint
11,10..14,13 and own footprint15,10..18,13 distinguish closer geometry from named
ownership. Hand ledger: own wood960→448 after512load→704 after256unload;
ship0→512→256; rival2304 stays2304. PublicDemolish invalidates previously prepared
Load/Unload IDs, both must refuse NoBuildingAtTile with byte-identical state.
Existing short cargo predicate/fraction/data fixtures cover the other E4/E6
refusal/partial/alternate-definition seams. This releases no dependent genuine
voyage acceptance; review and raw focused gate remain pending.

Independent E6 measurements use short labelled synthetic legal fixtures, with
no timing threshold or production counter added. Cargo fixtures use1/16/64 extra
islands and2/17/65 target-city warehouses,16 paired one-ton loads/unloads each.
Each pair restores all cities' stocks, ship holds, money and PRNG; rival and remote
balances are checked individually. Timed spans contain World::apply only. Static
caller review must verify direct island/stable-ID map resolution, bounded cargo
hold selection and the configured digital-disc outline; timings alone cannot
prove asymptotic locality or count map comparisons. Route fixtures are128/256/512
all-water squares with progressively longer legal diagonals,8 repeated queries
each, deterministic coarse/fine pops and working bytes compared separately from
index-build/query nanoseconds. A fine-budget1 counterexample must refuse with no
partial route while the legal default query executes. These are local API cost
measurements, not late-game acceptance or an original-map geometry substitute.

The first independent-companion gate at5892da6e stopped during compilation:
CitySpec.stock is optional, so the scale fixture must index its explicitly assigned
vector through value(). No selected tests executed; canonical-debug-01 remains a
failed build. The focused correction changes fixture initialization only.

Canonical independent02 at8b285645 is FAIL16selected/15passes/1failed, all other
outcome categories0. Boundary, route-scale and13 existing short cargo controls
pass. Independent review found that the64-building fixture's12-column grid
places the final row beyond its80×40 island; the World::create refusal correctly
exposed invalid fixture geometry. A14-column grid keeps origins22..74/20..36
and all4×4 footprints inside the island. Fix the fixture, retain02, and rerun the
affected local cargo-scale case only; unaffected15 passes remain their exactpin.

The corrected cargo-scale gate03 at6a17d647 passes1/1, all other categories0,
with all48paired samples across legal1/16/64 extra-island fixtures. It supersedes
only the failed cargo-scale fixture, not the whole16-test gate. Review also caught
a route-oracle gap: equality against another call proves determinism, not path
relevance. The strengthened route check independently walks every run, validates
water/bounds/corners, exact endpoint and hand Chebyshev lower bound96/224/480.
Removing the last run stays legal but fails that endpoint oracle. Gate04 reruns
only this changed route-scale case; earlier raw passes remain scoped to theirpin.

## Existing typed command recipe and Windows boundary (pending genuine completion)

Use the original `data/maps/candidates/ship-start-v2/lan5-ship-a.json`, seat0,
registry/content digests from E1/content-inputs.json, unchanged holds and map.
Explicit city_booking=true/save_billed_goods=true and house_updates=false is the
accounting test configuration, not a new application default. State133 currently
loses the founding warehouse cost: this recipe is blocked at the reviewed
compatibility boundary; it is not a completed Windows playtest.

A consumer posts existing `sim::Command{PlayerId{0}, body}` through
`PlayRunner::post_command`; receipt means queued, not accepted. Re-read stable
FigureId/CityRef/BuildingId from the latest snapshot after each accepted command.
Never cache City/Building pointers or choose a rival warehouse by visibility.
`MoveCmd` needs this ship's actual order ID plus `ShipArrived` before founding or
cargo. Match outcome player/order/ship, distinguish route refusal/failure.

1. Select original owned ship1, type1502, with crew2 and nested donkey3. Initial
   holds in order: wood50t, food10t, tools50t, tools30t. Gold30000. Start cityless.
2. Move to tile257,134, Q24.8 centre65920,34432. Actual E2 source arrival is
   proved after86000ticks. Found1302 using rotation0, origin254,137 and click255,138;
   the existing coast trials choose orientation. Derive the created warehouse ID
   and CityRef on island0 from the successful result; do not assume serial/city0.
   Gold−350, wood−12t/tools−5t aboard, source stock0wood.
3. Unload only tools2t from hold2 to that warehouse, transferred512raw. Build801
   at238,128, gold−150/tools−2t and wood−0. Reuse the legal road_path selection
   between actual forester and warehouse, dirt2520,20road tiles/gold100. Full road
   command coordinates must come from the executed recording (not yet published).
4. Advance normally until actual harvest/raw→wood→home cart→source stock arrives.
   Source must have0inherited wood; no editing or all-wood unload. LoadCmd fields
   ship1, source warehouse ID, preferred hold0, wood42, whole tons floor(stock/256),
   subject to remaining hold room; exact actual transfer, source fraction and all
   four ordered holds must be shown. Do not set timing/quantity from E1's copied
   planning result (448raw receipt/1t export is not a genuine-run fixed input).
5. Planned destination is island1 dock452,349 (Q24.8 centre115840,89472), click456,353,
   Found origin455,352/rotation0. E1 validated a copied world and exact legal route;
   real second arrival/founding is NOT proved yet. PublicFound costs350gold/12twood/
   5ttools, leaving a distinct same-owner CityRef. Unload1ttool from hold2. Build1310
   at450,355 is initially refused for missingwood unchanged. Unload produced freight
   from hold0, then retry same building:50gold/1twood/1ttool. Verify height/footprint.
6. Planned return uses dock257,134. A second load must exceed the entire previously
   made unexported wood pipeline, so further real output is necessary; derive its
   quantity from actual ledger, respect hold room/stock flooring. Return to452,349,
   unload through the second actual warehouse ID and record exact debit=credit.

Existing window modes are Select/Found/Unload, and existing commands_file.cpp
reads/writes found/move/stop/unload/buildc/demolishc. There is no `load` text token or
Load UI mode in this pin. Typed LoadCmd is already wire-encodable and post_command
can queue it; Windows owns adding any live control/line adapter. This task does not
introduce a parallel DSL or edit UI. Existing BuildCmd handles forest/roads/receiver;
public SetProductionLineCmd is used only on the separate no-production control.

Finite tools remain the eventual construction bound. No household population,
food cycle/needs fulfillment, stage advancement, salt/cloth/hunting chain or mineral
survey is established by this freight itinerary. House updates and economy stay
independently default-off. Nav uses bounded original-style coarse/fine queries,
acceptance radius/density stamps; it does not prove physical ship collision,
congestion avoidance/replanning, automated routes, network MP or late-game cost.

Gate04 at5a9be8ef stopped during compilation before selected execution: doctest
requires parentheses around a compound && assertion. Gate05 corrects only that
test expression. Both failed compile attempts remain failures with no executed
selection. Cargo timing has48pairs/96commands; route timing has24queries and
only1index-build sample peredge (repeated in each row, not8independent builds).
Typed Load outcomes already expose transferred_88 through the generic runner;
Windows owns the missing gesture and text-session parse/write adapters.

### Bounded checkpoint, not full MAP-EXPAND acceptance

At tested637da167 the current-source short ASan/UBSan gate passes16/16, all
failed/empty/skipped/incomplete/SAN categories0; six required invocations separately
pass1/1. Actual compile includes address/undefined/float-cast-overflow sanitizers.
This closes only the independent cargo/route subset after Debug review; it does
not replace current-source combined two-island sailing. E0/E1 approved, E2 NOT
APPROVED due founding history loss, E3–E7 full acceptance remains open. No runtime
fix or identity change landed. Complete per-run counts, first failures, hashes,
source pins and remaining dependency are in the
[bounded handoff](../orchestration/reviews/2026-11-06-map-expand1.md).
