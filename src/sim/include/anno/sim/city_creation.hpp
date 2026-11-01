// City creation, registration, naming and the Kontor counter as internal, headless operations
// (task MAP-2 packages C and D; contract: docs/sim/city-creation.md, reviewed before this code).
//
// Each operation is a plan/commit pair. prepare() is const: it mutates nothing and never advances
// the world PRNG (the name draw is peeked on a copy of the generator). commit() re-checks what the
// plan depended on and either applies every effect or reports StalePlan and changes nothing. There
// is no command, codec alternative or Reject value here: a later founding command will call these,
// after the payment preparation (founding_payment.hpp) and before the building and territory steps.
//
// Not modelled (docs/sim/city-creation.md section 7): the city's player mask and the island's
// derived words (relation table and consumers unknown), the island byte stored for the owner's
// colour, five further stamp words, the messages.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include <anno/formats/result.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/world.hpp>

namespace anno::sim {

enum class CityRefusal : std::uint8_t {
    UnknownIsland,
    UnknownPlayer,
    InvalidMetadata,  ///< stamp above u16, zero city slots, or an out-of-range cursor
    OwnCityOnIsland,  ///< the owner already has a city on this island
    NoCitySlot,       ///< no usable slot (the neutral value is never allocated)
    UnusableNameData, ///< an empty name list, or an entry containing NUL
    StalePlan,        ///< the world changed between prepare and commit
};

enum class KontorRefusal : std::uint8_t {
    UnknownIsland,
    CounterOverflow, ///< the counter is UINT32_MAX (the original wraps at 16 bits)
    StaleKontor,     ///< the counter changed between prepare and commit
};

struct CityRequest {
    IslandId island;
    PlayerId owner;
    /// The city stamp (city property 0x23): the caller's u16 tick stamp, above 65535 is refused.
    std::uint32_t stamp = 0;
    /// The world's name list (property 0x13, sub-list STAEDTE): nullopt = no list, no name and no
    /// draw; present = exactly one draw `mod` the list size. An empty list is refused.
    std::optional<std::span<const std::string>> names;
};

/// What commit applies, and what it checks again.
struct CityPlan {
    IslandId island;
    PlayerId owner;
    std::uint16_t stamp = 0;
    CityNo number;
    std::string name;
    bool draws = false;
    // What the plan depended on:
    std::uint32_t rng_cursor = 0;
    std::int32_t city_cursor = -1;
    std::uint32_t owner_cities = 0;
};

struct KontorPlan {
    IslandId island;
    std::uint32_t id = 0;      ///< the counter's old value: the building's KONTID
    std::uint32_t counter = 0; ///< the counter at prepare time
};

struct CityCreation {
    [[nodiscard]] static formats::Result<CityPlan, CityRefusal> prepare(const World& world,
                                                                        const CityRequest& req);
    /// Applies the plan: the city in its slot (owner, stamp, name), the allocation cursor, and the
    /// single name draw when the plan has one. Returns the city number.
    [[nodiscard]] static formats::Result<CityNo, CityRefusal> commit(World& world,
                                                                     const CityPlan& plan);

    /// The Kontor identifier step of City::Place for a new market: reads the counter only.
    [[nodiscard]] static formats::Result<KontorPlan, KontorRefusal>
    prepare_kontor(const World& world, IslandId island);
    /// Takes the old value as the id and increments the counter (checked). The caller stores the
    /// returned id in the building (PlacementRecord::kontor_id).
    [[nodiscard]] static formats::Result<std::uint32_t, KontorRefusal>
    commit_kontor(World& world, const KontorPlan& plan);
};

} // namespace anno::sim
