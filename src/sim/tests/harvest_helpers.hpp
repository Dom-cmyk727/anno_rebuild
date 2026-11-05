// A small synthetic forester world for the harvest tests (MAP-ECON-1 E2-E4, docs/sim/forestry.md).
// Every number is a data-defined fixture value, not an engine rule: the farm type stands in for the
// original forester 801 (rate 14, need 32, 5 t output, two workers, action radius 5, 3 x 3) and the
// worker type for figure 102 (speed 0.01 -> 33554, capacity 1 t, layer 2, wait 13.0 -> 6500 ticks,
// ROUTETYPE MAEHER = 0x2a). The tree types are a mature tree (WARE[0] BAUM) whose `id - 1` is a
// growing ROHSTWACHS type, an uncuttable mature tree whose `id - 1` is ROHSTOFF, and an obstacle.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/building.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/world.hpp>

#include "world_access.hpp"

namespace harvest_test {

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;

inline constexpr sim::TypeId kForester{9301};  // HOLZ from BAUM, 2 workers of kWorker, radius 5
inline constexpr sim::TypeId kGrainFarm{9302}; // KORN from GETREIDE, 1 worker of kReaper, radius 3
inline constexpr sim::TypeId kHerdFarm{
    9303};                                   // WOLLE from GETREIDE, 1 worker of kHerder (WEIDETIER)
inline constexpr sim::TypeId kMarket{9304};  // a market with one cart of kCart
inline constexpr sim::TypeId kRoad{9305};    // a road (flag 0x4000)
inline constexpr sim::TypeId kSapling{9310}; // ROHSTWACHS, the young stage of kTree
inline constexpr sim::TypeId kTree{9311};    // ROHSTOFF, WARE[0] BAUM: a mature tree
inline constexpr sim::TypeId kDeadWood{9312};  // ROHSTOFF, WARE[0] UNUSED: the `id - 1` of kOddTree
inline constexpr sim::TypeId kOddTree{9313};   // ROHSTOFF, WARE[0] BAUM: an uncuttable tree
inline constexpr sim::TypeId kStalks{9320};    // ROHSTWACHS, the young stage of kGrain
inline constexpr sim::TypeId kGrain{9321};     // ROHSTOFF, WARE[0] GETREIDE: a ripe field
inline constexpr sim::TypeId kSmallFarm{9330}; // a small farm: its line-0 worker is job 0x2a too
inline constexpr sim::TypeId kSmallFarm2{9331}; // another small farm: ALKOHOL from KARTOFFEL, NAHRUNG from GRAS
inline constexpr sim::TypeId kPotatoYoung{9326}; // ROHSTWACHS, the young stage of kPotato
inline constexpr sim::TypeId kPotato{9327};      // ROHSTOFF, WARE[0] KARTOFFEL: a ripe potato field
inline constexpr sim::TypeId kWall{9340}; // a tile object of walk code 3: closed on every layer

inline constexpr std::uint32_t kWorker = 102;  // the forester's worker (route MAEHER 0x2a)
inline constexpr std::uint32_t kReaper = 9401; // another data-defined harvester (route ERNTE 0x29)
inline constexpr std::uint32_t kHerder = 9402; // a farm figure of another route (WEIDETIER 46)
inline constexpr std::uint32_t kCart = 220;

/// The worker's facts: speed round(0.01f * 0.2f * 2^24) = 33554 (one tile per 500 ticks), 1 t,
/// layer 2, 6500 ticks, route 0x2a.
inline sim::FigureType worker_type(std::uint32_t id = kWorker, std::uint32_t capacity = 256,
                                   std::uint32_t route = 0x2a) {
    sim::FigureType f{id, 33554, capacity, 2, 6500};
    f.route = route;
    return f;
}

/// The goods id of `name` in the original goods table (the synthetic registries use it).
inline sim::GoodsId goods_id(std::string_view name) {
    const std::vector<sim::GoodsInfo> goods = sim::original_goods();
    for (std::size_t i = 0; i < goods.size(); ++i) {
        if (goods[i].name == name) {
            return static_cast<sim::GoodsId>(i);
        }
    }
    FAIL("no goods " << std::string(name));
    return 0;
}

/// A tile-object type: PRODTYPE 225 ROHSTOFF or 226 ROHSTWACHS, WARE[0] = `ware` (0 = UNUSED),
/// 1 x 1, walk code 2 (HEAD[0x2f] = 2: enterable on the workers' layer like open ground, as the
/// shipped trees, docs/re/forestry.md 2.11).
inline sim::BuildingType resource_type(sim::TypeId id, std::uint32_t prodtype, sim::GoodsId ware) {
    sim::BuildingType t;
    t.id = id;
    t.prodtype = prodtype;
    t.cls = sim::RuntimeClass::Plain;
    t.head_raw.assign(56, 0);
    t.head_raw[0x2f] = 2;
    if (ware != 0) {
        t.output_goods = {ware};
    }
    return t;
}

/// A tile object closed on every layer of the synthetic walk tables (walk code 3: only codes 0, 2
/// and 4 are open there, walk_cells.cpp synthetic_walk_tables).
inline sim::BuildingType wall_type() {
    sim::BuildingType t = resource_type(kWall, 225, 0);
    t.head_raw[0x2f] = 3;
    return t;
}

/// The registry. `worker_capacity` is the forester worker's capacity (a larger data-defined
/// capacity repeats the harvest, docs/sim/forestry.md E-14).
inline std::shared_ptr<const sim::TypeRegistry> registry(std::uint32_t worker_capacity = 256) {
    sim::TypeRegistry::Builder b;
    b.add_farm(kForester, "HOLZ", "BAUM", 14, 32, 5, 2)
        .footprint(kForester, 3, 3)      // 801's 3 x 3
        .entrances(kForester, {{11, 9}}) // (1.8333, 1.5): 801's entrance, its centre tile
        .action_radius(kForester, 5)
        .figure_ids(kForester, {kWorker})
        .add_farm(kGrainFarm, "KORN", "GETREIDE", 37, 80, 7, 1)
        .entrances(kGrainFarm, {{9, 9}})
        .action_radius(kGrainFarm, 3)
        .figure_ids(kGrainFarm, {kReaper})
        .add_farm(kHerdFarm, "WOLLE", "GETREIDE", 37, 80, 5, 1)
        .entrances(kHerdFarm, {{9, 9}})
        .figure_ids(kHerdFarm, {kHerder})
        .add_small_farm(kSmallFarm, "KORN", "WOLLE", "GETREIDE", "GRAS", 13, 4, 128, 50, 4)
        .footprint(kSmallFarm, 3, 3)
        .entrances(kSmallFarm, {{9, 9}})
        .action_radius(kSmallFarm, 3)
        .figure_ids(kSmallFarm, {kWorker})
        .add_small_farm(kSmallFarm2, "ALKOHOL", "NAHRUNG", "KARTOFFEL", "GRAS", 13, 4, 128, 50, 4)
        .footprint(kSmallFarm2, 3, 3)
        .entrances(kSmallFarm2, {{9, 9}})
        .action_radius(kSmallFarm2, 3)
        .figure_ids(kSmallFarm2, {kWorker})
        .add_market(kMarket, kCart, 1, 8)
        .entrances(kMarket, {{9, 15}})
        .add_plain(kRoad, 193)
        .flags_word(kRoad, sim::kRoadFlag)
        .add(resource_type(kSapling, 226, 0))
        .add(resource_type(kTree, 225, goods_id("BAUM")))
        .add(resource_type(kDeadWood, 225, 0))
        .add(resource_type(kOddTree, 225, goods_id("BAUM")))
        .add(resource_type(kStalks, 226, 0))
        .add(resource_type(kGrain, 225, goods_id("GETREIDE")))
        .add(resource_type(kPotatoYoung, 226, 0))
        .add(resource_type(kPotato, 225, goods_id("KARTOFFEL")))
        .add(wall_type())
        .figure_type(worker_type(kWorker, worker_capacity, 0x2a))
        .figure_type(worker_type(kReaper, 3 * 256, 0x29))
        .figure_type(worker_type(kHerder, 256, 46))
        .figure_type(sim::FigureType{kCart, 53687, 5 * 256, 3, 500});
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    return built.value();
}

/// Island 0 at (100, 100), 32 x 32, city 0 of player 0; island 1 at (200, 100), 16 x 16, city 0 of
/// player 1 (another owner).
struct Rig {
    sim::ScenarioSpec spec;
    sim::ScenarioSpec::CitySpec& city(std::size_t island = 0) {
        return spec.islands[island].cities[0];
    }
    void object(std::int32_t x, std::int32_t y, sim::TypeId type, std::size_t island = 0) {
        const auto& is = spec.islands[island];
        spec.islands[island].objects.push_back(sim::ScenarioSpec::TileObjectSpec{
            static_cast<std::uint32_t>((y - is.y0) * static_cast<std::int32_t>(is.w) + (x - is.x0)),
            type.v, 0});
    }
    void place(sim::TypeId type, std::int32_t x, std::int32_t y,
               std::optional<sim::Payload> payload = std::nullopt, std::size_t island = 0) {
        sim::ScenarioSpec::BuildingSpec b;
        b.type = type;
        b.x = x;
        b.y = y;
        b.payload = std::move(payload);
        city(island).buildings.push_back(std::move(b));
    }
};

inline Rig make_rig() {
    Rig r;
    r.spec = syn::empty_scenario(77);
    for (std::uint32_t p = 0; p < 2; ++p) {
        sim::ScenarioSpec::PlayerSpec ps;
        ps.id = sim::PlayerId{p};
        r.spec.players.push_back(ps);
    }
    sim::ScenarioSpec::IslandSpec a = syn::plain_island(0, 100, 100, 32, 32);
    a.owner_runs = {{0, 32U * 32U}};
    a.cities.push_back(syn::plain_city(0, 0));
    r.spec.islands.push_back(std::move(a));
    sim::ScenarioSpec::IslandSpec b = syn::plain_island(1, 200, 100, 16, 16);
    b.owner_runs = {{0, 16U * 16U}};
    b.cities.push_back(syn::plain_city(0, 1));
    r.spec.islands.push_back(std::move(b));
    return r;
}

inline sim::World create(const Rig& r, const std::shared_ptr<const sim::TypeRegistry>& types,
                         const sim::Rules& rules = sim::Rules{}) {
    auto w = sim::World::create(r.spec, types, sim::SimConfig{}, rules);
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    return std::move(w).value();
}

/// The figures of the registry, in slot order.
inline std::vector<const sim::Figure*> figures(const sim::World& w) {
    std::vector<const sim::Figure*> out;
    for (const auto& f : w.figures().list.slots) {
        if (f.has_value()) {
            out.push_back(&*f);
        }
    }
    return out;
}

inline std::uint32_t draws_at(const sim::SimOutput& out, sim::DrawSite site) {
    std::uint32_t n = 0;
    for (const sim::DrawRecord& d : out.draws) {
        n += d.site == site ? d.count : 0U;
    }
    return n;
}

} // namespace harvest_test
