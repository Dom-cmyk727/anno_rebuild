#include <cstdint>
#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

#include <anno/sim/synthetic.hpp>
#include <anno/sim/walk_cells.hpp>

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
    b.walk_tables(synthetic_walk_tables());
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

formats::Result<SyntheticTypes, std::string> make_transport_types() {
    auto base = make_synthetic_types();
    if (!base) {
        return base.error();
    }
    SyntheticTypes s = std::move(base).value();
    s.market = TypeId{9021};
    s.carrier_weaver = TypeId{9022};
    TypeRegistry::Builder b;
    b.walk_tables(synthetic_walk_tables());
    for (const BuildingType& t : s.registry->types()) {
        b.add(t);
    }
    b.flags_word(s.road, 0x4000)
        .add_market(s.market, 220, 1, 8)
        .entrances(s.market, {{9, 15}})
        .entrances(s.weaver, {{3, 3}})
        .entrances(s.farm, {{3, 3}})
        .add_workshop(s.carrier_weaver, "STOFFE", "WOLLE", 6, 11, 5, 1)
        .entrances(s.carrier_weaver, {{3, 3}})
        .action_radius(s.carrier_weaver, 16)
        .figure_ids(s.carrier_weaver, {203})
        // round(0.016f · 0.2f · 2^24) = 53687, round(0.012f · 0.2f · 2^24) = 40265; waits
        // round(1.0 · 500) and round(2.0 · 500) ticks.
        .figure_type(FigureType{220, 53687, 5 * 256, 3, 500})
        .figure_type(FigureType{203, 40265, 3 * 256, 0, 1000});
    auto built = b.build();
    if (!built) {
        return built.error();
    }
    s.registry = std::move(built.value());
    return s;
}

formats::Result<SyntheticTypes, std::string> make_growth_types() {
    auto base = make_synthetic_types();
    if (!base) {
        return base.error();
    }
    SyntheticTypes s = std::move(base).value();
    s.field = TypeId{9011};
    s.ripe_field = TypeId{9012};
    s.forest = TypeId{9013};
    s.ripe_forest = TypeId{9014};
    s.grass = TypeId{9015};
    s.ripe_grass = TypeId{9016};
    TypeRegistry::Builder b;
    b.walk_tables(synthetic_walk_tables());
    for (const BuildingType& t : s.registry->types()) {
        b.add(t);
    }
    // A growing object (PRODTYPE 226 ROHSTWACHS or 227 ROHSTDUERR: tile objects, no
    // list) and its successor (225 ROHSTOFF, not growing).
    const auto growing = [&](TypeId id, std::uint32_t prodtype, std::uint32_t period,
                             std::uint32_t stages, std::uint32_t shift) {
        BuildingType t;
        t.id = id;
        t.prodtype = prodtype;
        t.has_prod = true;
        t.prod.kind = BuildingClass::GrowingItem;
        t.prod.growth_period = period;
        t.prod.stages = stages;
        t.prod.stage_shift = shift;
        b.add(std::move(t));
        b.add_plain(TypeId{id.v + 1}, 225);
    };
    growing(s.field, 226, 15500, 5, 2);
    growing(s.forest, 226, 24000, 5, 2);
    growing(s.grass, 227, 1000, 5, 0);
    auto built = b.build();
    if (!built) {
        return built.error();
    }
    s.registry = std::move(built.value());
    return s;
}

SimConfig population_config() {
    SimConfig cfg;
    cfg.house_groups = 2;
    return cfg;
}

formats::Result<SyntheticTypes, std::string> make_population_types() {
    auto base = make_synthetic_types();
    if (!base) {
        return base.error();
    }
    SyntheticTypes s = std::move(base).value();
    const std::optional<GoodsId> food = s.registry->goods_by_name("NAHRUNG");
    const std::optional<GoodsId> leather = s.registry->goods_by_name("LEDER");
    const std::optional<GoodsId> cloth = s.registry->goods_by_name("STOFFE");
    if (!food || !leather || !cloth) {
        return std::string("make_population_types: the goods table lacks NAHRUNG, LEDER or STOFFE");
    }
    HouseGroup pioneers;
    pioneers.number = 0;
    pioneers.wohnmax = 8;
    pioneers.no_downgrade = true;
    pioneers.needs = {{*food, true, 41}, {*leather, true, 16}};
    pioneers.vermehr = {-16, -9, 0, 12, 32};
    HouseGroup settlers = pioneers;
    settlers.number = 1;
    settlers.no_downgrade = false;
    settlers.needs.push_back({*cloth, true, 20});
    TypeRegistry::Builder b;
    b.config(population_config());
    b.walk_tables(synthetic_walk_tables());
    b.house_groups({pioneers, settlers});
    for (const BuildingType& t : s.registry->types()) {
        b.add(t);
    }
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

ScenarioSpec::CitySpec plain_city(std::uint32_t no, std::uint32_t owner) {
    ScenarioSpec::CitySpec c;
    c.no = CityNo{no};
    c.owner = PlayerId{owner};
    return c;
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
    is.cities.push_back(plain_city(0, 0));
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
    is.cities.push_back(plain_city(1, 1));
    spec.islands.push_back(std::move(is));
    return spec;
}

ScenarioSpec g1_scenario(const SyntheticTypes& types, std::uint32_t seed) {
    ScenarioSpec spec = basic_scenario(seed);
    ScenarioSpec::CitySpec& city = spec.islands[0].cities[0];
    ScenarioSpec::BuildingSpec weaver;
    weaver.type = types.weaver;
    weaver.x = 110;
    weaver.y = 110;
    {
        Workshop w = make_workshop(types.registry->find(types.weaver)->prod);
        w.in[0] = 5 * kTon; // 5 t of wool
        weaver.payload = w;
    }
    ScenarioSpec::BuildingSpec farm;
    farm.type = types.farm;
    farm.x = 120;
    farm.y = 110;
    {
        Farm f;
        f.raw = 5 * kTon; // 5 t of raw material
        farm.payload = f;
    }
    city.buildings = {weaver, farm};
    return spec;
}

formats::Result<SyntheticTypes, std::string> make_build_types() {
    auto base = make_synthetic_types();
    if (!base) {
        return base.error();
    }
    SyntheticTypes s = std::move(base).value();
    const std::optional<GoodsId> tools = s.registry->goods_by_name("WERKZEUG");
    const std::optional<GoodsId> wood = s.registry->goods_by_name("HOLZ");
    const std::optional<GoodsId> bricks = s.registry->goods_by_name("ZIEGEL");
    if (!tools || !wood || !bricks) {
        return std::string("make_build_types: the goods table lacks WERKZEUG, HOLZ or ZIEGEL");
    }
    // Gold, tools, wood, bricks (tons) and the footprint of each type.
    struct Cost {
        TypeId id;
        std::uint32_t gold, tools, wood, bricks, w, h;
    };
    const Cost costs[] = {
        {s.weaver, 300, 3, 4, 0, 1, 1},      {s.farm, 200, 2, 3, 3, 1, 1},
        {s.fur_trapper, 100, 5, 6, 0, 2, 3}, {s.small_farm, 250, 2, 4, 0, 1, 1},
        {s.iron_mine, 1200, 5, 15, 0, 1, 1}, {s.pioneer_house, 0, 0, 3, 0, 2, 2},
        {s.kontor, 350, 5, 12, 0, 2, 2},     {s.market_stall, 50, 1, 1, 0, 1, 1},
    };
    TypeRegistry::Builder b;
    b.walk_tables(synthetic_walk_tables());
    for (const BuildingType& t : s.registry->types()) {
        BuildingType copy = t;
        for (const Cost& c : costs) {
            if (c.id != t.id) {
                continue;
            }
            copy.cost_gold = c.gold;
            copy.cost_goods.clear();
            // Ascending goods order (WERKZEUG 0x29, HOLZ 0x2a, ZIEGEL 0x2b), as the
            // type reader stores them; amounts in 8.8 tons.
            for (const auto& [goods, tons] : {std::pair{*tools, c.tools}, std::pair{*wood, c.wood},
                                              std::pair{*bricks, c.bricks}}) {
                if (tons != 0) {
                    copy.cost_goods.push_back(CostEntry{goods, tons << 8});
                }
            }
            copy.width = c.w;
            copy.height = c.h;
        }
        b.add(std::move(copy));
    }
    auto built = b.build();
    if (!built) {
        return built.error();
    }
    s.registry = std::move(built.value());
    return s;
}

ScenarioSpec g2_scenario(const SyntheticTypes& types, std::uint32_t seed) {
    ScenarioSpec spec = g1_scenario(types, seed);
    std::vector<std::uint32_t> stock(kOriginalWarehouseGoods, 0);
    if (const auto tools = types.registry->goods_by_name("WERKZEUG");
        tools && *tools < stock.size()) {
        stock[*tools] = 13 * kTon;
    }
    if (const auto wood = types.registry->goods_by_name("HOLZ"); wood && *wood < stock.size()) {
        stock[*wood] = 30 * kTon;
    }
    spec.islands[0].cities[0].stock = std::move(stock);
    return spec;
}

namespace {

ScenarioSpec::BuildingSpec building_at(TypeId type, std::int32_t x, std::int32_t y,
                                       std::optional<Payload> payload = std::nullopt) {
    ScenarioSpec::BuildingSpec b;
    b.type = type;
    b.x = x;
    b.y = y;
    b.payload = std::move(payload);
    return b;
}

/// A research column of SimConfig::research_entries bytes with `done` researched (state
/// byte 3: bit 0 researchable, bit 1 researched) and `open` only researchable (1).
std::vector<std::uint8_t> research_bytes(std::initializer_list<std::uint32_t> done,
                                         std::initializer_list<std::uint32_t> open) {
    std::vector<std::uint8_t> r(SimConfig{}.research_entries, 0);
    for (const std::uint32_t id : open) {
        if (id < r.size()) {
            r[id] = 1;
        }
    }
    for (const std::uint32_t id : done) {
        if (id < r.size()) {
            r[id] = 3;
        }
    }
    return r;
}

} // namespace

ScenarioSpec g3_scenario(const SyntheticTypes& types, std::uint32_t seed) {
    ScenarioSpec spec = two_island_scenario(seed);
    const TypeRegistry& reg = *types.registry;
    // Research ids of the bow maker's goods (BGruppen [WAREN] Forsch, OQ-034): BOGEN 4,
    // ARMBRUST 9.
    spec.players[0].research = research_bytes({4}, {9});
    spec.players[1].research = research_bytes({4, 9}, {});

    const auto weaver = [&]() {
        Workshop w = make_workshop(reg.find(types.weaver)->prod);
        w.in[0] = 5 * kTon;
        return Payload{w};
    };
    const auto bow_maker = [&]() {
        WeaponsWorkshop w = make_weapons_workshop(reg.find(types.bow_maker)->prod);
        w.in[0] = 5 * kTon; // HOLZ
        w.in[1] = 5 * kTon; // SEILE
        return Payload{w};
    };
    Farm farm;
    farm.raw = 5 * kTon;
    // 2 t: the trapper runs out of raw stock before its store is full, so a trapper becomes
    // due (and finds no prey).
    Farm trapper;
    trapper.raw = 2 * kTon;
    SmallFarm small = make_small_farm(reg.find(types.small_farm)->prod);
    small.raw[0] = 5 * kTon;
    small.raw[1] = 5 * kTon;

    ScenarioSpec::IslandSpec& is0 = spec.islands[0];
    // The deposit of basic_scenario at (110, 120) with 2 t and a second one at (112, 121)
    // with 1 t, both without the reserve flag (OQ-035 b: who clears it is open; a deposit
    // without it is used up by any mine).
    is0.deposits[0].remaining = 2 * kTon;
    is0.deposits[0].reserve = false;
    ScenarioSpec::DepositSpec second;
    second.goods = "EISENERZ";
    second.x = 112;
    second.y = 121;
    second.remaining = 1 * kTon;
    second.reserve = false;
    is0.deposits.push_back(second);
    is0.cities[0].buildings = {
        building_at(types.weaver, 110, 110, weaver()),
        building_at(types.farm, 120, 110, Payload{farm}),
        building_at(types.fur_trapper, 130, 110, Payload{trapper}),
        building_at(types.small_farm, 140, 110, Payload{small}),
        building_at(types.bow_maker, 150, 110, bow_maker()),
        building_at(types.iron_mine, 110, 118),
        building_at(types.pioneer_house, 125, 125),
    };
    spec.islands[1].cities[0].buildings = {
        building_at(types.bow_maker, 305, 205, bow_maker()),
        building_at(types.weaver, 310, 205, weaver()),
    };
    return spec;
}

ScenarioSpec g4_scenario(const SyntheticTypes& types, std::uint32_t seed) {
    ScenarioSpec spec = empty_scenario(seed);
    spec.settings.prop7_offset = 1000;
    ScenarioSpec::PlayerSpec human;
    human.id = PlayerId{0};
    ScenarioSpec::PlayerSpec debtor;
    debtor.id = PlayerId{1};
    debtor.kind = 12;
    debtor.money = -2900;
    debtor.name = {'A', 'I'};
    ScenarioSpec::PlayerSpec natives;
    natives.id = PlayerId{2};
    natives.kind = 14;
    natives.volk = 2;
    natives.money = 5000;
    spec.players = {human, debtor, natives};

    ScenarioSpec::IslandSpec is0 = plain_island(0, 100, 100, 24, 24);
    is0.owner_runs = {{0, 24U * 24U}};
    ScenarioSpec::CitySpec city0 = plain_city(0, 0);
    Workshop w = make_workshop(types.registry->find(types.weaver)->prod);
    w.in[0] = 5 * kTon;
    Farm f;
    f.raw = 5 * kTon;
    city0.buildings = {building_at(types.weaver, 110, 110, Payload{w}),
                       building_at(types.farm, 120, 110, Payload{f})};
    is0.cities.push_back(std::move(city0));

    ScenarioSpec::IslandSpec is2 = plain_island(2, 300, 100, 16, 16);
    is2.climate = 1;
    is2.cities = {plain_city(0, 0), plain_city(1, 1)};

    ScenarioSpec::IslandSpec is4 = plain_island(4, 300, 300, 16, 16);
    is4.climate = 0;
    is4.cities = {plain_city(0, 1), plain_city(1, 2)};

    spec.islands = {std::move(is0), std::move(is2), std::move(is4)};
    return spec;
}

ScenarioSpec g5_scenario(const SyntheticTypes& types, std::uint32_t seed) {
    ScenarioSpec spec = empty_scenario(seed);
    spec.settings.prop6 |= 0x80U; // the 7.5 minute event off (world property 6 bit 0x80)
    ScenarioSpec::PlayerSpec p;
    p.id = PlayerId{0};
    spec.players.push_back(p);
    ScenarioSpec::IslandSpec is = plain_island(0, 100, 100, 60, 44);
    is.owner_runs = {{0, 60U * 40U}, {3, 60U * 4U}};
    is.objects = {{3U * 60U + 2U, types.field.v, 0}, {42U * 60U + 58U, types.forest.v, 0}};
    ScenarioSpec::CitySpec city0 = plain_city(0, 0);
    city0.buildings = {building_at(types.kontor, 131, 123)};
    is.cities = {std::move(city0), plain_city(3, 0)};
    spec.islands.push_back(std::move(is));
    return spec;
}

ScenarioSpec g6_scenario(const SyntheticTypes& types, std::uint32_t seed) {
    ScenarioSpec spec = empty_scenario(seed);
    ScenarioSpec::PlayerSpec p;
    p.id = PlayerId{0};
    spec.players.push_back(p);
    ScenarioSpec::IslandSpec is = plain_island(0, 100, 100, 40, 32);
    is.owner_runs = {{0, 40U * 32U}};
    for (const auto& [x, y] :
         {std::pair{111, 113}, {111, 114}, {112, 114}, {113, 114}, {114, 114}}) {
        is.objects.push_back(ScenarioSpec::TileObjectSpec{
            static_cast<std::uint32_t>((y - 100) * 40 + (x - 100)), types.road.v, 0});
    }
    ScenarioSpec::CitySpec city0 = plain_city(0, 0);
    Workshop weaver = make_workshop(types.registry->find(types.weaver)->prod);
    weaver.out[0] = 5 * kTon;
    Farm farm;
    farm.out = 5 * kTon;
    farm.raw = 5 * kTon;
    city0.buildings = {building_at(types.market, 110, 110),
                       building_at(types.weaver, 111, 115, Payload{weaver}),
                       building_at(types.farm, 114, 115, Payload{farm}),
                       building_at(types.carrier_weaver, 106, 114)};
    std::vector<std::uint32_t> stock(kOriginalWarehouseGoods, 0);
    stock[9] = 30 * kTon; // WOLLE
    city0.stock = std::move(stock);
    is.cities = {std::move(city0)};
    spec.islands.push_back(std::move(is));
    return spec;
}

ScenarioSpec g7_scenario(const SyntheticTypes& types, std::uint32_t seed) {
    ScenarioSpec spec = empty_scenario(seed);
    ScenarioSpec::PlayerSpec p;
    p.id = PlayerId{0};
    spec.players.push_back(p);
    ScenarioSpec::IslandSpec is = plain_island(0, 100, 100, 40, 32);
    is.owner_runs = {{0, 40U * 32U}};
    ScenarioSpec::CitySpec city0 = plain_city(0, 0);
    // 5 residents (the constructor's), food at s = 128: trunc(41 * 128 * 5 * 128 / 1536).
    House house;
    house.stock = {41U * 128U * 5U * 128U / 1536U, 0};
    city0.buildings = {building_at(types.pioneer_house, 110, 110, Payload{house})};
    std::vector<std::uint32_t> stock(kOriginalWarehouseGoods, 0);
    for (std::size_t g = 0x29; g <= 0x2d && g < stock.size(); ++g) {
        stock[g] = 20 * kTon; // the settler-house materials
    }
    stock[26] = 10 * kTon; // NAHRUNG
    stock[27] = 5 * kTon;  // LEDER
    city0.stock = std::move(stock);
    is.cities = {std::move(city0)};
    spec.islands.push_back(std::move(is));
    return spec;
}

} // namespace anno::sim::synthetic
