# Early needs: cloth and leather from resources to households (MAP-NEEDS-2)

Contract and acceptance index. Status 2026-11-05: **N0 inventory only** (nothing implemented). Base: local head `67158af4` (stream `kStateHashVersion` 129, `kPrngTimelineVersion` 5, `Rules::house_updates` default false). Packet: `cache/map-needs2/map-needs-2.md` (staged from origin/main `04f578f6`). Binary findings: `docs/re/hunter.md`; the shared harvest job and the small farm: `docs/sim/small-farm-supply.md` sections 2-4; forestry: `docs/sim/forestry.md`.

Marks: **D** read from original bytes, **P** parsed from the files, **A** adaptation (named), **U** unread.

## 1. Evidence identities (N0)
All under `game/app` (private, never committed): `Anlagen.dat` sha256 `b2317a5c...dd75`, `FigurenHeader.dat` `29fa1f05...68a8`, `Data/BGruppen.dat` `63e876fd...9e849e3b`... see list below, `AnnoWelt.dll` `f220a878...a6ae2`, `AnnoGame.dll` `ac721220...7db`; scenes `Scenes/Tutorial/02Trade01.sze` `90123783c1ed4fb8cab32cedbb3612e7f6210e207d91be8c53a65b83513480e8`, `Scenes/0100Kampagne01/S01_07_01.sze` `04f60b235f3137d83eb2078d1a727ca5e3c737d973f5a87a26c2ea6725e58d62`, `Scenes/0011SingleEndlosAddon/endless02.sze` `734151588c5cd5854fc809c14fbd65cc7afa8050676690a87d5a2c42ae711f97`.
Full hashes: Anlagen `b2317a5c24b8c9fe796cf40f3fedaefcfa30ad3f185df9e69b2dca6a03d8dd75`; FigurenHeader `29fa1f0528fb337d9a61e8b73169281fd05bb27efa6df65638580a4c36c468a8`; BGruppen `63e876fd3afedca214b3d9cc45f022f2c595f473839690e96f34e12ed7849e3b`; AnnoWelt `f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`; AnnoGame `ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc`.
Raw probes (git-ignored): `cache/map-needs2/N0/` (`n0_inventory.txt`, `n0_scenes.txt`, `n0_figs.txt`, sources `n0_*.cpp`).

## 2. Host (N0)
Apple Silicon Mac, 18 cores, 48 GiB, load average about 3.5 at the inspection, Ninja/CMake from `cache/mac-tools`, AppleClang; builds `build/mac-headless` (Debug), `build/mac-asan`, `build/mac-release`. At most 4 build jobs and 2 test processes; other executors' jobs and the keep-awake assertion are not touched.

## 3. Producer inventory (P, from the parsed registry)
| Stage | Type (example ids, not allowlists) | Class / tick | Goods and rates (8.8 per step) | Figure (route, layer, capacity) |
|---|---|---|---|---|
| Sheep farm | 807 `SCHAFFARM` 3x3 r5 | Farm / TickKind::Farm | GRAS 87 -> WOLLE 6, capacity 1792, max figures 5 | 103: route 46 (animal job 0x2e), layer 4, 768 |
| Cotton plantation (not interchangeable) | 819 `PLANTAGE`, 3614 | Farm | BAUMWOLLE -> WOLLE | 133: route 42 (worker) |
| Weaver | 1001 (and 1005) `HANDWERK` 3x3 r16 | Workshop / Workshop | WOLLE 11 -> STOFFE 6 (1005: 17 -> 13) | 203: route 51 (carrier), layer 0, 768 |
| Hunter | 805 `JAGDHAUS` 3x3 r8 | Hunter / **None (inert)** | FLEISCHTIER 57 -> NAHRUNG 43 + TIERHAEUTE 26 | 108: route 12, layer 2, 256 |
| Fur trapper (different) | 824 `PELZJAEGER` | FurTrapper (never sends) | PELZTIER -> PELZE | 108: route 12 |
| Tanner | 1023 `HANDWERK` | Workshop | TIERHAEUTE 12 -> LEDER 7 | 203 |
| Cloth/leather stall | 1314 `MARKTSTAND` 1x1 r4 | MarketStall | outputs STOFFE, LEDER | cart 101 |
| Grass objects | 2, 21, 23, 25, 27, 69, 71, 73, 79, 81, 83, 355 (`ROHSTOFF`, WARE GRAS 69) | tile objects | - | - |
| Other pasture/animal farms of the same class | 820 (GRAS -> FLEISCH), 3011..3702 `WEIDETIER` (GRAS -> NAHRUNG), route 46 | Farm | - | affected by any change keyed on route 46 |

Goods ids from the registry: GRAS 69, WOLLE 9, STOFFE 33, TIERHAEUTE 18, LEDER 27, FLEISCHTIER 137, NAHRUNG 26, BAUMWOLLE 61, PELZE 38.

## 4. Stage status
| Stage | Status | Evidence |
|---|---|---|
| Sheep farm tick, gate, dispatch eligibility and the dispatch draw | **existing** (shared Farm tick) | `driver.cpp` TickKind::Farm; `forestry.md` 3.1 |
| Sheep farm spawn: figure 103, job from route 46, cargo goods = input 0 (GRAS) | **existing, but incorrect for this job**: `consume()` sets the harvest state only for `harvest_job` (0x29/0x2a); a Farm figure of job 0x2e is spawned **without** a harvest state, so it does nothing useful and the counter is released only by whatever ends it | `driver.cpp` `s.harvest = harvest_job(s.job) && Farm` |
| Shared harvest job for animals (job 0x2e, limit 20, cargo = input goods) | **existing for the SmallFarm home** (S2/S3) | `harvest.cpp`, `small-farm-supply.md` 2.3 |
| Farm home accepted by the harvest body, credit to raw store | **existing** (Farm payload has one `raw`, credit slot 0) | `harvest.cpp` home check, credit slot |
| Raw GRAS -> WOLLE conversion and output store (`farm_step`) | **existing** | `production.cpp farm_step`, `production.md` |
| Weaver input acquisition by the carrier (job 0x33) from a Farm output | **existing** (Farm is a source: `has_output_store`, `out_of`, `reserved_of`); needs an own cloth test | `transport.cpp` |
| Weaver conversion and output store | **existing** (Workshop) | `production.cpp` |
| Output collection to the warehouse | **existing** (carrier return, `transport.cpp` 886) | |
| Warehouse -> stall 1314 -> household purchase of STOFFE/LEDER | **existing and exercised**: `S01_07_01` pioneer house 509 supplied with leather and cloth (`population-live.md` section 23-24); the walker reaches 1314 since OQ-463 | |
| Hunter tick, target search, job 0xd body, wildlife objects, sweep | **unread / missing** (`docs/re/hunter.md`) | |
| Tanner conversion and delivery | **existing** (Workshop, same path as the weaver) | |
| Variants through data (no id list) | to implement: keyed on the figure type's route and the building type's goods | |

## 5. Scene choice and stock audit
Survey of all 50 scenes (`n0_scenes.txt`), every city with sheep farm, weaver, hunter or tanner (50 city rows).
**Cloth canary: `Tutorial/02Trade01.sze`, island 7, city 1, owner 0** (the town of MAP-SUPPLY-1). Buildings (imported): sheep farms 807 at (278,344) and (273,354) (grass objects in the two search squares: 43 tiles), weaver 1001 at (279,350), market halls 1301 at (305,335), (282,350) and 1302 at (304,356), food stall 1310 at (305,339), Inn 1335, cattle/other farms 801, 812 x2, small farms 809 x4, hunter 805 at (284,346) (the island has **no** `HIRSCH` entries), 13 houses (types 501, 503, 505 x2, 507 x2, 509 x2, 520, 521, 522 x2, 524). **No cloth stall** (1314) stands there: the proof builds one by a public `BuildCmd` (INFRA_EVER by the registry; placement and payment are the ordinary rules; imported buildings prove production, not construction eligibility). City stock: WOLLE 0, STOFFE 0, TIERHAEUTE 0, LEDER 0, NAHRUNG 17,408 (other goods listed in `n0_town.cpp` output); so cloth and leather are **zero at the start** and nothing preloaded can explain household supply. No other town or supplier on the island (the only other city of the scene is on island 3, owner 2, with its own preloaded STOFFE 7,680). Unaudited until N3: in-flight cargo at tick 0 (none expected, no figures are imported), the housing need predicate for STOFFE (BGruppen BauFirst EVER for NAHRUNG, LEDER, STOFFE, `population-inputs.md` B1) and whether these 13 houses reach a new stall (to be measured by the walker as in the S01_07_01 evidence).
**Leather/hunting candidates** (needs wildlife, section 6): cities with hunter, tanner and `HIRSCH` entries on their island and zero TIERHAEUTE stock: `0100Kampagne01/S01_07_01.sze` island 51 (hunters 8, tanners 4, HIRSCH 532, LEDER 22,016 preloaded), `S01_03_01.sze` island 35 (hunters 9, tanners 4, HIRSCH 34, LEDER 15,872), `S01_02_01.sze` island 18 (HIRSCH 635, preloaded 40,960 of each good). All have preloaded LEDER, so a leather proof needs the causal-control design of the packet (a labelled stock-limited fixture plus an unmodified-original proof).

## 6. Hunting dependency list (N0; blocks N4/N5 only)
1. **Wildlife is not created by the sim** (`driver.cpp` FurTrapper note, `state.md` O-12/O-25): no `FLEISCHTIER` carrier exists in the sim.
2. The scenario islands hold animals as a `HIRSCH` record (P: 668 records, 13,919 eight-byte entries in the 50 scenes; no island template has one). Layout of the entry, relation to goods 137 and the object's later life: **U** (`docs/re/hunter.md`).
3. Target search `0x10071da0(8)` body **U**; hunter tick `0x10069f40` arithmetic only partly read; hunter figure job `0xd` body **U**; claim/kill/return/cleanup **U**; renewal (fauna sweep) **U**.
4. Hunter and fur trapper share figure 108 / route 12; their behaviors must not be conflated.
Verdict: a faithful hunting implementation needs reads 2 and 3 (no new policy decision is needed to *read* them). **Hunting is expected to finish PARTIAL unless those reads complete early; cloth does not depend on it.** No wildlife lifecycle will be invented.

## 7. Cloth plan (N1-N3)
N1: reference (`tools/early_needs_ref.py`) and vectors for the sheep dispatch/visit/credit/conversion/transfer chain, committed first. N2: harvest state for the Farm figure whose type route is the animal job (keyed on route and goods; no id list), credit slot 0, the existing weaver path under test with debit/credit/reservation checks, an alternate data-defined registry producer/consumer. N3: the canary of section 5 with a producer-disabled control that still runs.

## 8. Acceptance table (claims; mutation and assertion named before implementation)
| ID | Claim | Positive case | Decisive counterexample | Source evidence | Intended mutation -> assertion | Reviewer disposition |
|---|---|---|---|---|---|---|
| C1 | A sheep-farm figure (type route 46) gets the shared animal harvest job with cargo = input 0 and limit 20 | vector: 807 farm, one grass tile at distance d, figure walks, cuts, +256 | the same farm with route 42 gets the worker limit 0; a SmallFarm animal keeps credit slot 1 | `driver.cpp` consume; `small-farm-supply.md` 2.3; AnnoGame 0x1006ee06-0x1006ee56; FigurenHeader 103 | spawn without the harvest state -> wool made > 0 and raw credit exactly 256 per cut | pending |
| C2 | Credit lands in the Farm's one raw store (slot 0), exact 8.8 | vector | credit to `out` instead of `raw` | `harvest.cpp`; Farm struct | credit to out -> raw == 256 * cuts, out unchanged by the cut | pending |
| C3 | GRAS -> WOLLE per step 87 in / 6 out, capacity 1792, saturation | existing `farm_step` vectors; one conversion per 5760/128-tick period | full output store stops the step, status kept | `production.md`; type 807 data | change the step rate -> exact wool count after N steps | pending |
| C4 | Weaver carrier loads WOLLE from the farm output once, reservation then debit, no double credit | transport test: farm out 6, weaver input after delivery | second carrier cannot load reserved wool; failed send leaves the farm unchanged | `transport.cpp` | skip the reservation release -> reserved == 0 after the trip | pending |
| C5 | Weaver converts 11 -> 6 per step and the carrier brings STOFFE to the warehouse | workshop vectors; chain test | input 10 gives no step | `production.cpp` Workshop | change in/out -> exact STOFFE | pending |
| C6 | Warehouse STOFFE reaches a 1314 stall and households buy it | S01_07_01 evidence; N3 canary | control with farms removed: supply stops when the (labelled) small store is gone | `population-live.md` 23-24 | disable the sheep path -> cloth bought > 0 only with it | pending |
| C7 | The town ledger reconciles (wool made = cloth inputs + stock + cargo; cloth made = stall/store + purchases + cargo; unexplained 0) with only this town's producers counted | N3 | counting the other island's farms must change the figure | N3 ledger | a double credit -> unexplained != 0 | pending |
| C8 | Home removal, repeated worker end, interrupted walk, full output, failed send are clean | tests | worker counter released once | `forestry.md`; harvest lifecycle tests | skip the one-time release -> counter == 0 | pending |
| C9 | Alternate data-defined producer/consumer works; unsupported behaviour refuses | N2 registry | a route the engine does not know is refused | N2 | - | pending |
| H1-H5 | Hunting (target, kill, return, two outputs, cleanup) | **blocked** by section 6 | - | `docs/re/hunter.md` | - | not started |

## 9. Identity (proposal, decided in N6)
Enabling the animal job for Farm-class homes changes every world that contains a Farm-class pasture or sheep building (types 807, 820, 3011-3702 and others with route 46): their state evolves differently, so the stream hash moves. 129 is local and unpublished, but SUPPLY-1 was accepted with 129 pinned, so this work is expected to be **130 / timeline 5** (no new draw site; the dispatch draw already exists) with `tools/sim_stream.py` updated only if a field is added (none expected). To be confirmed with the version contract at N6.
