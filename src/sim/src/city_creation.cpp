#include <limits>
#include <utility>

#include <anno/sim/city.hpp>
#include <anno/sim/city_creation.hpp>
#include <anno/sim/draw.hpp>

namespace anno::sim {

namespace {

const Island* island_of_id(const World& world, IslandId id) {
    const auto& table = world.islands();
    return id.v < table.size() && table[id.v].has_value() ? &*table[id.v] : nullptr;
}

std::uint32_t cities_of(const Island& island, PlayerId owner) {
    std::uint32_t n = 0;
    for (const std::optional<City>& c : island.cities) {
        n += c.has_value() && c->owner == owner ? 1U : 0U;
    }
    return n;
}

/// The first usable empty slot after `cursor`, wrapping, over all slots once (the cursor slot
/// last); the neutral value is skipped. D AnnoWelt 0x10096816-0x1009684d.
std::optional<std::uint32_t> next_slot(const Island& island, const SimConfig& cfg) {
    const std::int64_t slots = cfg.city_slots;
    std::int64_t i = island.city_cursor;
    for (std::int64_t k = 0; k < slots; ++k) {
        i = (i + 1) % slots;
        if (static_cast<std::uint32_t>(i) == cfg.neutral_owner) {
            continue;
        }
        if (!island.cities[static_cast<std::size_t>(i)].has_value()) {
            return static_cast<std::uint32_t>(i);
        }
    }
    return std::nullopt;
}

} // namespace

formats::Result<CityPlan, CityRefusal> CityCreation::prepare(const World& world,
                                                             const CityRequest& req) {
    const Island* island = island_of_id(world, req.island);
    if (island == nullptr) {
        return CityRefusal::UnknownIsland;
    }
    if (world.players().find(req.owner) == nullptr) {
        return CityRefusal::UnknownPlayer;
    }
    const SimConfig& cfg = world.config();
    if (req.stamp > std::numeric_limits<std::uint16_t>::max() || cfg.city_slots == 0 ||
        island->city_cursor < -1 ||
        island->city_cursor >= static_cast<std::int64_t>(cfg.city_slots) ||
        island->cities.size() != cfg.city_slots) {
        return CityRefusal::InvalidMetadata;
    }
    const std::uint32_t own = cities_of(*island, req.owner);
    if (own != 0) {
        return CityRefusal::OwnCityOnIsland;
    }
    const std::optional<std::uint32_t> slot = next_slot(*island, cfg);
    if (!slot) {
        return CityRefusal::NoCitySlot;
    }
    CityPlan plan;
    plan.island = req.island;
    plan.owner = req.owner;
    plan.stamp = static_cast<std::uint16_t>(req.stamp);
    plan.number = CityNo{*slot};
    plan.rng_cursor = world.rng().cursor();
    plan.city_cursor = island->city_cursor;
    plan.owner_cities = own;
    if (req.names.has_value()) {
        const std::span<const std::string> names = *req.names;
        if (names.empty()) {
            return CityRefusal::UnusableNameData;
        }
        for (const std::string& n : names) {
            if (n.find('\0') != std::string::npos) {
                return CityRefusal::UnusableNameData;
            }
        }
        // Peek: the draw is taken on a COPY of the generator (the live one is untouched).
        WorldRng probe = world.rng();
        GlobalDraw door{probe};
        const std::uint16_t drawn = door.next(DrawSite::CityName);
        plan.name = names[drawn % names.size()];
        plan.draws = true;
    }
    return plan;
}

formats::Result<CityNo, CityRefusal> CityCreation::commit(World& world, const CityPlan& plan) {
    const SimConfig& cfg = world.config();
    if (plan.island.v >= world.islands_.size() || !world.islands_[plan.island.v].has_value()) {
        return CityRefusal::StalePlan;
    }
    Island& island = *world.islands_[plan.island.v];
    // A plan is made by prepare(); a hand-built one naming the neutral value is never applied.
    if (plan.number.v == cfg.neutral_owner || world.rng_.cursor() != plan.rng_cursor ||
        island.city_cursor != plan.city_cursor || island.cities.size() != cfg.city_slots ||
        plan.number.v >= island.cities.size() || island.cities[plan.number.v].has_value() ||
        cities_of(island, plan.owner) != plan.owner_cities) {
        return CityRefusal::StalePlan;
    }
    // Every check has passed: apply. (D 0x100967e0 order: the city, ring[number], the cursor, the
    // stamp, then the name and its single draw.)
    City city = make_city(plan.number, plan.owner, cfg);
    city.tick_stamp = plan.stamp;
    city.index.stamp = plan.stamp;
    if (plan.draws) {
        GlobalDraw door{world.rng_};
        static_cast<void>(door.next(DrawSite::CityName)); // the value the plan peeked
        city.name = plan.name;
    }
    island.cities[plan.number.v] = std::move(city);
    island.city_cursor = static_cast<std::int32_t>(plan.number.v);
    return plan.number;
}

formats::Result<KontorPlan, KontorRefusal> CityCreation::prepare_kontor(const World& world,
                                                                        IslandId island_id) {
    const Island* island = island_of_id(world, island_id);
    if (island == nullptr) {
        return KontorRefusal::UnknownIsland;
    }
    if (island->kontor_counter == std::numeric_limits<std::uint32_t>::max()) {
        return KontorRefusal::CounterOverflow;
    }
    return KontorPlan{island_id, island->kontor_counter, island->kontor_counter};
}

formats::Result<std::uint32_t, KontorRefusal> CityCreation::commit_kontor(World& world,
                                                                          const KontorPlan& plan) {
    if (plan.island.v >= world.islands_.size() || !world.islands_[plan.island.v].has_value()) {
        return KontorRefusal::UnknownIsland;
    }
    Island& island = *world.islands_[plan.island.v];
    // A plan is made by prepare_kontor(), where the id IS the counter; any other pair is stale.
    if (island.kontor_counter != plan.counter || plan.id != plan.counter) {
        return KontorRefusal::StaleKontor;
    }
    if (island.kontor_counter == std::numeric_limits<std::uint32_t>::max()) {
        return KontorRefusal::CounterOverflow;
    }
    island.kontor_counter = plan.counter + 1;
    return plan.id;
}

} // namespace anno::sim
