// The resident fetch job 0x35 (MAP-POP-LIVE-1 L2; docs/sim/population-live.md sections 1 step 3, 2,
// 3, 12). A house sends a walker (resident_dispatch, the spawn block of the House tick), the walker
// searches a stand or public building for its home's need, walks there, buys at most two cargo
// slots (resident_buy, L1), walks home, hands every non-empty slot to the house and ends through
// the existing end path (resident_tick, the figure body). Integer only; the body draws nothing, the
// dispatch draws exactly once.
//
// The House tick does NOT call resident_dispatch yet (L4): the House dispatch of the driver stays a
// no-op and Rules::house_updates stays false. Evidence: D AnnoWelt.dll 0x10076341-0x10076397 (the
// spawn block), the base send 0x10072d20 / 0x10072b50, the job's steps 0x1005b170 (A), 0x10056480
// (B arrival), 0x10057ae0 / 0x10057d70 (C); the unread edges (stall wait, stall attach, flag bits
// 4/5, first-walkable-entrance rule) are the contract's P-8 simplifications.
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <anno/sim/building.hpp>
#include <anno/sim/city.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/draw.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/resident.hpp>
#include <anno/sim/tick.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

/// The job code of the house walker (D: the House tick's send 0x10072d20 passes job 0x35).
inline constexpr std::uint32_t kJobResident = kJobResidentCode;

/// D 0x10076341-0x10076371: a House sends a walker only while its lowest need score is below this.
// Original default; the configurable path is a SimConfig field (not added in this task).
inline constexpr std::int64_t kWalkerSpawnBelow = 102;


/// Whether the resident body runs figures of this job.
[[nodiscard]] bool resident_runs_job(std::uint32_t job);

/// The list mask of the candidate query and the arrival lookup (D: city mask 0x21 = the stall list
/// and the public list), derived from the layout's roles, never a literal.
[[nodiscard]] std::uint32_t resident_stand_mask(const ListLayout& layout);

/// The house's side of a transaction (needs, lookup, goods, E, residents, stock) for `house` of
/// type `type`; nullopt when the type is no house of the registry's group table.
[[nodiscard]] std::optional<HouseFacts> house_facts_of(const SimConfig& cfg,
                                                       const TypeRegistry& types,
                                                       const BuildingType& type,
                                                       const House& house);

/// The supplier model of a building type (class, the two WARE goods; the stall flags stay 1 and the
/// Inn's tavern stock 0: nothing flips or fills them yet, contract section 2 "Enabled").
[[nodiscard]] SupplierModel supplier_model_of(const BuildingType& type);

/// Why a walker was not sent after the draw.
enum class ResidentRefusal : std::uint8_t {
    None,
    NoFigureType, ///< the registry has no figure type with the picked id
    NoEntrance,   ///< the house type has no entrance point (the send finds no reachable entrance)
    NoFreeSlot,   ///< no empty walker slot although the count said there was room (inconsistent)
};

/// What one spawn block did.
struct ResidentDispatch {
    /// lowest < kWalkerSpawnBelow and fewer walkers than the type allows.
    bool condition = false;
    /// The world PRNG was drawn (exactly when `condition`).
    bool drew = false;
    std::uint16_t draw = 0;
    /// k = ((r & 1) + y + x + serial) mod variants (unsigned, x, y, serial as u16).
    std::uint32_t variant = 0;
    ResidentRefusal refusal = ResidentRefusal::None;
    /// The send: a SpawnRequest of job 0x35 (resident = true, home_counter = the walker slot taken
    /// at the merge). Absent when the condition was false or the send was refused.
    std::optional<SpawnRequest> spawn;
};

/// The spawn block of the House tick (population-live.md section 1, step 3). `lowest` is the
/// kernel's lowest need score, `walkers_used` the number of non-empty walker slots of the house.
/// When lowest < 102 and walkers_used < the type's walker maximum it draws the world PRNG ONCE
/// (site HouseWalkerTarget, BEFORE the send and whatever the send does), picks the figure type of
/// group 0 by k and sends: refused when the type has no entrance, no free slot or the registry no
/// figure type (house flag bits 4/5 are not modelled in the payload and read as clear). A false
/// condition draws nothing. The caller pushes `spawn` into Effects::spawns; the merge takes the
/// slot.
[[nodiscard]] ResidentDispatch resident_dispatch(DrawSource& draw, const Island& island,
                                                 const City& city, const Building& house,
                                                 const BuildingType& type,
                                                 const TypeRegistry& types, std::int64_t lowest,
                                                 std::uint32_t walkers_used);

/// The number of non-empty walker slots of a house.
[[nodiscard]] std::uint32_t resident_walkers_used(const House& house);

/// The chosen stand of step A.
struct StandChoice {
    BuildingId building;
    GoodsId goods = 0;
    std::int64_t score = 0;
    std::vector<TileXY> path;
};

/// Step A's choice (D 0x1005b170): the candidates are the buildings of the house's city in the
/// stand lists inside the house's centre square (radius = the house type's action radius) whose
/// house_choice is at least 12; the walk search from the house's entrances over the figure's layer
/// visits their entrances in path-cost order, and the stand is the reachable candidate with the
/// STRICTLY highest score (the first discovery wins a tie; not the nearest). nullopt = go to step
/// C. `walker` is the calling figure (its cargo and the cargo of the house's other walkers feed the
/// skip rule); a null `walker` has no cargo.
[[nodiscard]] std::optional<StandChoice> choose_stand(const SerialWorld& world,
                                                      const TickContext& ctx, const Figure* walker,
                                                      const Building& home,
                                                      const BuildingType& home_type, CityRef city,
                                                      std::uint32_t layer);

/// One call of the resident body on `fig` (a figure of job 0x35 with a ResidentState): it works
/// through u16(t - fig.stamp) ticks (the walk, the fixed wait), then sets the stamp to u16(t).
/// Returns false, changing nothing, for a figure whose type the registry does not know or that has
/// no resident state.
bool resident_tick(const TickContext& ctx, SerialWorld& world, Figure& fig);

} // namespace anno::sim
