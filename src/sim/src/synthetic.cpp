#include <cstdint>
#include <utility>
#include <vector>

#include <anno/sim/synthetic.hpp>

namespace anno::sim::synthetic {

formats::Result<SyntheticTypes, std::string> make_synthetic_types() {
    SyntheticTypes s;
    s.weaver = TypeId{9001};
    s.farm = TypeId{9002};
    s.fur_trapper = TypeId{9003};
    s.small_farm = TypeId{9004};
    s.bow_maker = TypeId{9005};
    s.iron_mine = TypeId{9006};
    s.pioneer_house = TypeId{9007};
    s.kontor = TypeId{9008};
    s.market_stall = TypeId{9009};
    s.road = TypeId{9010};
    TypeRegistry::Builder b;
    b.add_workshop(s.weaver, "STOFFE", "WOLLE", 6, 11, 5)
        .add_farm(s.farm, "KORN", "GETREIDE", 37, 80, 5)
        .add_fur_trapper(s.fur_trapper, "PELZE", "PELZTIER", 26, 25, 5)
        .add_small_farm(s.small_farm, "KORN", "WOLLE", "GETREIDE", "GRAS", 13, 4, 128, 50, 4)
        .add_weapons_workshop(s.bow_maker, "BOGEN", "ARMBRUST", "HOLZ", "SEILE", 9, 5)
        .add_mine(s.iron_mine, "EISENERZ", 21, 5)
        .add_plain(s.pioneer_house, 97)
        .add_plain(s.kontor, 35)
        .add_plain(s.market_stall, 161)
        .add_plain(s.road, 193);
    auto built = b.build();
    if (!built) {
        return built.error();
    }
    s.registry = std::move(built.value());
    return s;
}

WorldSettings synthetic_settings() {
    WorldSettings s;
    s.world_width = 512;
    s.world_height = 512;
    return s;
}

ScenarioSpec::IslandSpec plain_island(std::uint32_t slot, std::int32_t x0, std::int32_t y0,
                                      std::uint32_t w, std::uint32_t h) {
    ScenarioSpec::IslandSpec is;
    is.slot = IslandId{slot};
    is.x0 = x0;
    is.y0 = y0;
    is.w = w;
    is.h = h;
    is.climate = 2;
    return is;
}

ScenarioSpec empty_scenario(std::uint32_t seed) {
    ScenarioSpec spec;
    spec.settings = synthetic_settings();
    spec.seed = seed;
    return spec;
}

ScenarioSpec basic_scenario(std::uint32_t seed) {
    ScenarioSpec spec = empty_scenario(seed);
    ScenarioSpec::PlayerSpec p;
    p.id = PlayerId{0};
    spec.players.push_back(p);
    ScenarioSpec::IslandSpec is = plain_island(0, 100, 100, 64, 64);
    // Two neutral rows (64 tiles each), then city 0 to the end of the island.
    is.owner_runs = {{15, 128}, {0, 64U * 62U}};
    is.objects = {{200, 1201, 0}, {201, 1202, 3}, {4095, 1203, 7}};
    ScenarioSpec::DepositSpec d;
    d.goods = "EISENERZ";
    d.x = 110;
    d.y = 120;
    is.deposits.push_back(d);
    spec.islands.push_back(std::move(is));
    return spec;
}

ScenarioSpec two_island_scenario(std::uint32_t seed) {
    ScenarioSpec spec = basic_scenario(seed);
    ScenarioSpec::PlayerSpec p;
    p.id = PlayerId{1};
    p.kind = 12;
    p.money = 25000;
    p.name = {'A', 'I'};
    spec.players.push_back(p);
    ScenarioSpec::IslandSpec is = plain_island(3, 300, 200, 30, 20);
    is.climate = 6;
    is.owner_runs = {{1, 30U * 10U}};
    is.objects = {{0, 2001, 0}, {599, 2002, 1}};
    std::vector<std::uint8_t> height(30U * 20U);
    std::vector<std::uint16_t> gfx(30U * 20U);
    std::vector<std::uint8_t> fertility(30U * 20U);
    for (std::uint32_t i = 0; i < height.size(); ++i) {
        height[i] = static_cast<std::uint8_t>(i % 7U);
        gfx[i] = static_cast<std::uint16_t>(i * 3U);
        fertility[i] = static_cast<std::uint8_t>(i % 5U);
    }
    is.terrain = make_terrain(std::move(height), std::move(gfx), std::move(fertility));
    ScenarioSpec::DepositSpec d1;
    d1.goods = "GOLD";
    d1.x = 301;
    d1.y = 202;
    d1.remaining = 123456;
    d1.reserve = false;
    ScenarioSpec::DepositSpec d2;
    d2.goods = "STEINE";
    d2.x = 329;
    d2.y = 219;
    is.deposits = {d1, d2};
    spec.islands.push_back(std::move(is));
    return spec;
}

} // namespace anno::sim::synthetic
