// Local integer decisions only: docs/sim/wildlife-decisions.md. No World wiring.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <anno/formats/result.hpp>
#include <anno/sim/ids.hpp>

namespace anno::sim {
enum class WildlifeFault {
    InvalidInput,
    DrawsExhausted,
    Oq572CandidatesExhausted,
    Oq574CaveWithoutEntrance,
    Oq585ZeroHpPrey
};
struct WildlifeError {
    WildlifeFault fault;
    std::size_t consumed = 0;
};
template <class T> struct WildlifeDecision {
    T value;
    std::size_t consumed = 0;
};
template <class T> using WildlifeResult = formats::Result<WildlifeDecision<T>, WildlifeError>;

// The original 4x4 sweep has 16 samples, not a cap on world animals/content.
[[nodiscard]] WildlifeResult<std::uint32_t> wildlife_density(std::uint32_t candidates);
struct WildlifeHabitatFact {
    TypeId object{0}; // Zero is an empty tile, not the Id default sentinel.
    GoodsId goods = 0;
    std::uint32_t kind = 0;
};
// Eight geometric neighbours; growing-type substitution must already be resolved.
[[nodiscard]] WildlifeResult<bool> wildlife_habitat(WildlifeHabitatFact center,
                                                    std::span<const WildlifeHabitatFact> neighbors,
                                                    std::uint32_t property7);
struct WildlifeReplenishment {
    std::vector<std::uint32_t> chosen, remaining;
};
// Tokens are caller-local candidate positions. Output is selection order, not list visit order.
[[nodiscard]] WildlifeResult<WildlifeReplenishment>
wildlife_replenish(std::span<const std::uint32_t> candidates, std::uint8_t deficit,
                   std::span<const std::uint32_t> occupied, std::span<const std::uint32_t> draws);
struct WildlifeSpecies {
    TypeId type;
    GoodsId goods;
};
struct WildlifeCreation {
    std::int32_t position; // -1: first cave entrance, otherwise selector 0..3; no coordinates.
    std::uint32_t index;
    TypeId species;
    std::uint32_t inspected;
};
// Exactly the selected climate's 32-entry ring (draw &31), with widened content ids.
// Stops before manager creation/attach and their unresolved transitive callbacks.
[[nodiscard]] WildlifeResult<WildlifeCreation>
wildlife_create(bool cave, std::size_t entrances, std::span<const WildlifeSpecies> species,
                GoodsId saved, std::span<const std::uint32_t> draws);
struct WildlifeLoad {
    std::uint8_t timestamp;
    GoodsId saved;
};
// AnnoWelt 0x100946e9–0x10094705: raw u16 island stamp / 500; no draw.
[[nodiscard]] std::uint32_t wildlife_load_base(std::uint16_t raw_stamp);
// Base is already divided by 500, hence 0..131; do not divide it again.
[[nodiscard]] WildlifeResult<WildlifeLoad> wildlife_load(std::uint8_t entry4, std::uint32_t base,
                                                         std::uint8_t mode, GoodsId goods,
                                                         std::span<const std::uint32_t> draws);
enum class WildlifeEvent { Complete, Fallback, Idle };
struct WildlifeTransitionInput {
    WildlifeEvent event;
    std::uint8_t state;
    bool success;                     // Resolved callee outcome, not a simulated action.
    std::uint16_t quantity, capacity; // Quantity AFTER completed action effects.
    std::uint8_t variants;
};
struct WildlifeTransition {
    std::uint8_t next;
    std::optional<std::uint8_t> variant;
};
[[nodiscard]] WildlifeResult<WildlifeTransition>
wildlife_animal(WildlifeTransitionInput input, std::span<const std::uint32_t> draws);
[[nodiscard]] WildlifeResult<WildlifeTransition>
wildlife_wolf(WildlifeTransitionInput input, std::span<const std::uint32_t> draws);
struct WildlifeWalk {
    // Three candidates are the original local algorithm, not a world entity limit.
    std::array<std::uint32_t, 3> radii, angles;
    std::uint8_t attempt;
};
// Radii in sixteenths, angle-table indices only (OQ-582); no float positions/paths.
[[nodiscard]] WildlifeResult<WildlifeWalk> wildlife_walk(std::uint8_t attempt,
                                                         std::uint8_t behavior, std::uint8_t facing,
                                                         std::span<const std::uint32_t> draws);
[[nodiscard]] WildlifeResult<std::uint8_t> wildlife_route_old(std::uint16_t quantity,
                                                              std::span<const std::uint32_t> draws);
struct WildlifePreyLists {
    std::span<const TypeId> short_list, extended_list, extended_predators;
};
// Admission only, not nearest target/HP/death/reachability. Lists come from caller data.
[[nodiscard]] WildlifeResult<bool> wildlife_prey(TypeId predator, TypeId prey, std::uint32_t kind,
                                                 bool zero_hp_unresolved, WildlifePreyLists lists);
} // namespace anno::sim
