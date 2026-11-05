// The Tutorial island-7 town (city 1, player 0) as a MAP-SUPPLY-1 fixture: the imported farms, Inn
// and houses of docs/sim/small-farm-supply.md section 10. Needs ANNO1503_GAME_DIR.
#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/world.hpp>

#include "tutorial_town.hpp"
#include "world_access.hpp"

namespace island7 {

namespace sim = anno::sim;
using namespace tutorial_town;

inline constexpr std::uint32_t kIsland7 = 7, kCity1 = 1;
inline constexpr sim::GoodsId kFood = 26, kAlcohol = 29;
inline constexpr std::uint64_t kCheckpoint =
    150000; // before the first regrowth can complete (>= 150000 after a cut)

struct Chain {
    sim::World w;
    sim::TransportStats stats;
    sim::Island* isl = nullptr;
    sim::City* city = nullptr;
    sim::CityRef ref;
    sim::BuildingId farm_potato, farm_empty, farm_grass1, farm_grass2, inn;
    explicit Chain(sim::World&& world) : w(std::move(world)) {}
    Chain(const Chain&) = delete;
    Chain& operator=(const Chain&) = delete;
    [[nodiscard]] const sim::Building* at(sim::BuildingId id) const {
        return isl->find_building(id);
    }
    [[nodiscard]] const sim::SmallFarm& farm(sim::BuildingId id) const {
        return std::get<sim::SmallFarm>(at(id)->payload);
    }
    [[nodiscard]] const sim::Inn& inn_state() const { return std::get<sim::Inn>(at(inn)->payload); }
};

inline std::unique_ptr<Chain> start(const Original& o, bool producers) {
    auto c = std::make_unique<Chain>(tutorial_world(o, true));
    for (auto& i : sim::WorldTestAccess::islands(c->w)) {
        if (i && i->index.v == kIsland7) {
            c->isl = &*i;
            for (auto& ci : i->cities) {
                if (ci && ci->number.v == kCity1) {
                    c->city = &*ci;
                }
            }
        }
    }
    REQUIRE(c->city != nullptr);
    c->ref = sim::CityRef{c->isl->index, c->city->number};
    const auto id_at = [&](int x, int y, std::uint32_t type) {
        const sim::Building* b = c->isl->building_at(c->w.config().layout, c->city->number, x, y);
        REQUIRE(b != nullptr);
        REQUIRE(b->type.v == type);
        return b->id;
    };
    c->farm_potato = id_at(261, 344, 809);
    c->farm_empty = id_at(259, 352, 809);
    c->farm_grass1 = id_at(307, 349, 809);
    c->farm_grass2 = id_at(292, 333, 809);
    c->inn = id_at(301, 351, 1335);
    c->w.set_transport_stats(&c->stats);
    if (!producers) {
        std::vector<sim::TileXY> tiles{{261, 344}, {259, 352}, {307, 349}, {292, 333}};
        sim::Command demolish;
        demolish.issuer = c->city->owner;
        demolish.body = sim::DemolishCmd{c->ref, tiles};
        const auto res = c->w.apply(demolish);
        REQUIRE_MESSAGE(res.ok(), std::string(sim::reject_name(res.status)));
    }
    return c;
}

inline std::uint32_t potatoes_ripe(const Chain& c) {
    std::uint32_t n = 0;
    for (int y = 342; y <= 348; ++y) {
        for (int x = 259; x <= 265; ++x) {
            n += c.isl->tiles.object(c.isl->tile_index(x, y)) == 709 ? 1U : 0U;
        }
    }
    return n;
}

/// The cargo of every living figure of `goods` whose home is a building of this island's city (the
/// other town of the scenario has its own carts and carriers; ended figures' cargo is history, not
/// stock).
inline std::uint64_t cargo_in_flight(const Chain& c, sim::GoodsId goods) {
    std::uint64_t n = 0;
    for (const auto& s : c.w.figures().list.slots) {
        if (s.has_value() && !s->expiry_stamp.has_value() && s->cargo_goods == goods &&
            s->home.valid() && c.isl->find_building(s->home) != nullptr) {
            n += s->cargo_88;
        }
    }
    return n;
}

} // namespace island7
