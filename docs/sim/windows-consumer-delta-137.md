# Windows consumer delta, state 135 -> 137 (MAP-EXTRACT-2), head see the hand-back

Scope: what `src/app`, `src/render` and UI code see through the public headers of `src/sim/include/anno/sim/`.
Verification: every declaration below was read in the header at the cited line (head of this branch). Behavioural claims
are tied to the test that executes them. The whole tree (incl. `anno_app_tests`, `anno_render_tests`) builds and links
against these headers unchanged (full canonical Debug run, `cache/map-extract-2/t32_final2_debug.log`): **no symbol that
`src/app` or `src/render` uses was renamed or removed** (grep of `src/app`, `src/render` for the removed names: none).
"U" = not verified.

## 0. What a consumer must change
- **Nothing to compile.** The only removed public names are resident-walk internals (section 7); none is used outside `src/sim` and its tests (grep).
- **Behave:** (a) saves and recordings of 133/134/135 are refused (section 8); (b) `Payload` has ten alternatives, so an exhaustive `std::visit` over `Building::payload` needs a `Quarry` case (`building.hpp:116`, section 4); (c) iron and quarry types now place, so a UI that listed "Unsupported" types as unbuildable now sees `ConstructionDenied` for stage/research-locked ones (section 3); (d) there is NO `Exhausted` mine state: derive it (section 1).

## 1. Mines: `mine_facts()` (mine_facts.hpp)
```cpp
// mine_facts.hpp:16-29
enum class MineState : std::uint8_t { Unbound, SwitchedOff, Blocked, Working };
// mine_facts.hpp:31-54
struct MineFacts {
    bool bound = false;
    std::optional<std::uint64_t> deposit_index;      // index into Island::deposits
    std::int32_t ore_x = 0, ore_y = 0;               // tile of the ore object
    std::uint32_t remaining_88 = 0;                  // the deposit's RAW remaining, 8.8 tons (/256 = t), reserve included
    bool reserve = false;                            // the deposit's reserve flag
    bool depletes = false;                           // whether THIS mine uses the deposit up
    bool deep = false;                               // Mine::deep
    std::uint32_t output_88 = 0;
    std::optional<std::uint64_t> produced_88;        // since a TransportStats sink was installed; nullopt without one
    MineState state = MineState::Unbound;
};
// mine_facts.hpp:57
[[nodiscard]] std::optional<MineFacts> mine_facts(const World& world, BuildingId id);
```
- **Shallow vs deep (iron):** `deep` is `mine_is_deep(type.prod, research_id)` (production.hpp:439): 1205/1210 are shallow (stage 2C gate), 1216/1217 deep (research 57 gate), salt/gold/gems deep. `depletes = reserve ? (!deep && setting) : setting` with `setting = world.settings().depletes()` (settings.hpp:64, the add-on edition; mine_facts.cpp:40). So a shallow iron mine depletes only in the depleting setting; a deep mine and salt never do.
- **Reserve:** `kDepositReserve = 0x64000` (1,600 t, production.hpp:358). `deposit_remaining(const Deposit&, bool deep)` (production.hpp:363) is what the mine may still take: `remaining - kDepositReserve` for a shallow mine with the reserve flag (u32, clamped as the original), else `remaining`. A consumer can compute it from the facts: `sim::deposit_remaining(sim::Deposit{"", ore_x, ore_y, remaining_88, reserve}, deep)`.
- **Exhausted: there is no state for it.** An exhausted bound mine reports `MineState::Blocked` (its step reports MissingInput, which sets `Mine::blocked`; mine_facts.cpp:46-47, production.cpp:768/791), identical to a mine whose output store is full. A consumer derives exhaustion as: `facts.bound && facts.depletes && deposit_remaining(...) == 0` (a deposit the mine rebinds to a neighbour ore keeps it Working: executed by `iron interruption: a shallow deposit exhausted in the depleting setting idles with one notice and a neighbour ore keeps the mine working`).
- **The 0x15B notice:** raised once on the short step when an EISENERZ mine finds nothing left: `StepResult::notice` (production.hpp:138) becomes `SimEvent{kind = SimEventKind::Notice (tick.hpp:328), building, code = 0x15B (deep ore left) or 0x19}` (SimEvent.code, tick.hpp:363), read with `World::drain_events()` (world.hpp:305). Once per exhaustion (same test).
- `produced_88` is a diagnostic ledger of an installed sink, NOT saved state (mine_facts.hpp:48-52).
```cpp
// mine_facts.hpp:66-68 (read-only; the sim does NOT refuse a mine without ore in reach, it is created unbound and idle)
[[nodiscard]] std::optional<bool> mine_site_deposit_in_reach(const World& world, TypeId type,
        std::int32_t x, std::int32_t y, std::uint8_t rotation);
```
A host/UI may call it before issuing a `BuildCmd` to warn the player; nullopt for a non-mine type or an off-island tile (mine_facts.hpp:59-65).

## 2. Placement parameters (placement.hpp)
```cpp
// placement.hpp:93 and :96
std::vector<std::string> mountain_wares = {"SALZSTEIN", "EISENERZ"};   // goods NAMES, not ids
bool quarries = true;                                                   // quarry types place by their own HIGHMAP table
```
A mine type of the mountain class (flag word & 0x7000 == 0x3000) places only if its first output ware is in `mountain_wares` AND the registry holds ore evidence for it (a figure type BEWOHNTYPE 145 with that WARE; placement.hpp:83-92). Gold and gem mines are not in the list. Quarries (PRODTYPE 13, class Quarry) place with `quarries == true`; false restores the earlier `Unsupported`.

## 3. `Reject` answers (reject.hpp)
- `Reject::Unsupported` (reject.hpp:32): gold and gem mines (not in `mountain_wares`) and any mountain-class type without a table (placement.cpp classify).
- `Reject::ConstructionDenied` (reject.hpp:108): iron mines and quarries whose stage or research gate is not met (1205/1210 and 1201/1208 stage 2B/2C, 1216/1217 research 57, 1218/1220 and marble mason stage 3C=13). Executed: `salt mine: the refusal matrix with a legal neighbour per row` (quarries now answer ConstructionDenied there) and `quarry placement: a BuildCmd creates a quarry ...` (1218 denied until the stage bit is set).
- The eligibility query (build_availability.hpp, `World::construction_eligibility(PlayerId, TypeId)`, world.hpp:270) gives the status without issuing a command.

## 4. Quarry class and claim flags
```cpp
// building.hpp:59-66
struct Quarry {
    std::uint16_t last = 0; bool blocked = false; bool suspended = false;
    StatusTracker status; std::optional<bool> financial_active = true;
    std::vector<std::uint8_t> claims;   // 0 / 1 per ENTRANCE of the type (1201: 2, 1208/1218/1220: 1)
};
// building.hpp:116  using Payload = std::variant<Inert, Workshop, WeaponsWorkshop, Farm, SmallFarm, Mine, Market, House, Inn, Quarry>;
// building.hpp:238  [[nodiscard]] PayloadTag payload_tag_of(const BuildingType& type, const ClassInfo& cls);
// types.hpp:102     PayloadTag::Quarry = 9
```
- **No dedicated read-only accessor.** A consumer reads the flags with `world.find_building(id)` (world.hpp:331) and `std::get_if<sim::Quarry>(&b->payload)->claims`. A set flag means a mason worker walks to or waits at that entrance (it does not mean "busy building"; a quarry never depletes and holds no stock). `entrance_walk_points(b, type)` (transport.hpp:413) gives the sub-cell of entrance i, in the same order as `claims`.
- The class table is unchanged (Quarry still pairs tick None with Inert), so the registry digest did not move; `payload_tag_of` hands the Quarry class the new payload.
- Mason: figure route 44 / job 0x2c (`mason_job`, figure.hpp:113; `FarmFigureRoute::Mason`, figure.hpp:127). Its walk state is the harvest state; for job 0x2c only, `HarvestState::goal_col/goal_row` (figure.hpp:166) hold the chosen entrance sub-cell. Read-only for a renderer: `Figure::harvest->step` (ToTree = walking to the quarry, Cut = waiting, ToHome) and `route`.
- A quarry is a constant 256 raw stock source (`kQuarryStock88`, mason.hpp:25); it never depletes.

## 5. Diagnostics (all measurement only: not saved, not hashed; install with `World::set_transport_stats(TransportStats*)`, world.hpp:317)
| Struct (header:line) | Names | Meaning |
|---|---|---|
| `ForestryStats` (transport.hpp:124) | `quarry_lifted_88`, `mason_delivered_88`, `mason_lost_88`, `mason_searches`, `mason_searches_failed`, `quarry_claims_set`, `quarry_claims_cleared` (transport.hpp:163-168) | per quarry: cargo mason trips took; per mason home: delivered, lost with the figure, step-A searches and failed ones; claim writes issued |
| `OreLookupStats` (ore_lookup_stats.hpp:15-27), `TransportStats::ore_lookup` (transport.hpp:376) | `lookups`, `ore_objects_examined`, `in_reach`, `ware_matches`, `deposits_examined`, `relookup_fallbacks` | cost of a mine step's re-lookup over every ore object; `relookup_fallbacks` = the earlier deposit-table search was used (expected 0 where deposits come from ore objects) |
| `ResidentStats` (transport.hpp:173-188) | `home_walks`, `home_entrance` (map entrance index -> count), `no_path_home` | forward searches for a walk home; which entrance the search reached; failed searches (7,500-tick failure branch, goods lost) |

Pure helpers that are stable and header-declared: `mason_priority`, `mason_candidate_score`, `mason_candidate_stock`, `mason_stock_qualifies`, `mason_output_matches` (mason.hpp:32-52).

## 6. Resident statistics replaced (T3-Q2)
Removed from `ResidentStats` (transport.hpp): `reverse_rejected`, `endpoint_adapted`, `forward_used`. Added: `home_walks`, `home_entrance`. Removed from `resident_job.hpp`: `ResidentPathNote`, `resident_path_home`, `resident_route_legal`. grep of `src/app`, `src/render`: no use. Behaviour (not API): resident trips home take other routes and end on the entrance sub-cell centre; about 1.5 % of trips on the 7-city fixture end in the failure branch (OQ-556, U whether the original does the same on that stall).

## 7. Other behaviour a UI can see
- Iron ore and a working iron/tool chain, quarries and masons now run in every scene (the AI cities' masons too): per-scene producer counts moved (`quarry-bricks.md` Q3.4).
- The Notice stream (SimEventKind::Notice) carries 0x15B/0x19 for iron mines.

## 8. Save and recordings
- `kStateHashVersion = 137` (world.hpp:171, comment updated); it is the version word of a save (save.cpp:54) and of a replay log. A save or recording of another version is refused: `save: state stream version 135 is not supported (this build reads 137)` (save.cpp:155-157, replay.cpp:42/67/401). Executed: `iron identity137: real133, real134 and real135 archives refuse, each with its own diagnostic`, and the four fresh-process replays (`iron_chain_fresh_process`, `iron_chain_changed_command_fresh_process`, `salt_chain_fresh_process`, `salt_chain_changed_command_fresh_process`) run in the final gate.
- New saved fields inside 137 (no new number): the Quarry payload (tag 9) and `goal_col/goal_row` for job-0x2c figures (`docs/sim/quarry-bricks.md` Q3.1, `docs/sim/state.md` end of file). 136 belongs to the route lane and is not part of this identity.
