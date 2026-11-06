// Independent populated field bytes and hierarchical digests, authored before comparing C++.
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/state_fields.hpp>
namespace sim = anno::sim;
namespace {
struct Expected {
    std::vector<std::uint8_t> bytes;
    std::uint64_t hash;
};
std::map<std::string, Expected> expected() {
    std::ifstream in(std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "salt1" / "current135" /
                     "populated_fields.txt");
    REQUIRE(in.good());
    std::map<std::string, Expected> out;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        std::string name, hex, digest;
        REQUIRE(bool(row >> name >> hex >> digest));
        auto& e = out[name];
        e.hash = std::stoull(digest, nullptr, 16);
        for (std::size_t i = 0; i < hex.size(); i += 2)
            e.bytes.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    }
    REQUIRE(out.size() == 7);
    return out;
}
template <class T> void compare(const Expected& e, const T& value) {
    sim::WriteArchive bytes;
    sim::fields(bytes, value);
    CHECK(bytes.data() == e.bytes);
    sim::HashArchive hash;
    sim::fields(hash, value);
    CHECK(hash.finish() == e.hash);
}
sim::Figure actor(std::uint64_t id, std::uint32_t kind, sim::FigureId parent = {},
                  std::optional<std::uint32_t> crew = {}, bool animal = false, bool ship = false) {
    sim::Figure f;
    f.id = sim::FigureId{id};
    f.kind = kind;
    f.owner = sim::PlayerId{0};
    f.parent = parent;
    f.crew_slot = crew;
    f.stamp = 19;
    f.dir16 = 4;
    f.pos = animal ? sim::FixedPos{28416, 28416} : sim::FixedPos{900, 980};
    f.hp = ship ? 12000 : (kind == 718 ? 400 : 350);
    if (ship) {
        f.holds = {{41, 12801}, {42, 512}};
        f.state_word = 0x8000;
        f.nav = sim::NavMotion{};
        f.nav->last = 11;
        f.nav->pos_u[0] = 900 * sim::kNavPosUnitsPerQ8;
        f.nav->pos_u[1] = 980 * sim::kNavPosUnitsPerQ8;
        f.nav->res24[0] = 5;
        f.nav->res24[1] = 7;
        f.nav->health = 12000;
    } else if (parent.valid())
        f.holds = {{26, 7}};
    if (animal) {
        f.home = sim::BuildingId{1};
        f.home_counter = 1;
        f.job = 0x2e;
        f.cargo_goods = 69;
        f.cargo_88 = 256;
        f.harvest = sim::HarvestState{};
        f.harvest->step = sim::HarvestStep::Cut;
        f.harvest->start_delay = 0;
        f.harvest->wait = 4321;
        f.harvest->target = {113, 111};
    }
    return f;
}
} // namespace
TEST_CASE(
    "integration oracle: populated script permission actor navigation holds and animal scopes") {
    const auto e = expected();
    compare(e.at("header"), sim::StateHeader{});
    compare(e.at("accounts"),
            sim::CityAccounts{65000, 0x0102030405060708ULL, 105, 210, 4294967313ULL, 28});
    auto economy = sim::EconomyConfig{};
    economy.climate_names.push_back("CUSTOM");
    economy.tax_count = 4;
    compare(e.at("economy"), economy);
    sim::SimConfig config;
    config.economy = economy;
    std::vector<sim::GoodsInfo> priced{{"UNUSED"}, {"ALPHA"}, {"BETA"}};
    priced[1].prices = sim::GoodsPrices{{45, 45, 45, 50, 50, 50, 45, 17}, {0, 5, 10, 15}};
    priced[2].prices =
        sim::GoodsPrices{std::vector<std::uint32_t>(8, 7), std::vector<std::uint32_t>(4, 2)};
    sim::TypeRegistry::Builder price_builder;
    auto price_registry = price_builder.config(config).goods(priced).build();
    REQUIRE(price_registry);
    CHECK(price_registry.value()->digest() == e.at("goods_registry").hash);
    priced[1].prices->climate[7] = 18;
    CHECK(sim::TypeRegistry::digest_of({}, priced, sim::original_class_table()) !=
          e.at("goods_registry").hash);
    sim::Player p;
    p.index = sim::PlayerId{0};
    p.research.state.resize(65);
    p.research.progress.resize(65);
    p.permissions = {{sim::TypeId{70001}, 0}, {sim::TypeId{90001}, 255}};
    compare(e.at("player"), p);
    sim::FigureRegistry registry;
    registry.list.slots.resize(40);
    registry.list.hint = 8;
    registry.list.cursor = 9;
    registry.stamp = 19;
    registry.list.slots[37] = actor(1, 1503, {}, {}, false, true);
    registry.list.slots[2] = actor(2, 717, sim::FigureId{1}, 0);
    registry.list.slots[7] = actor(3, 718, sim::FigureId{2});
    registry.list.slots[8] = actor(4, 106, {}, {}, true);
    compare(e.at("figures"), registry);
    sim::ScriptRuntime r;
    r.source = "[OBJECT_DEFINE]\n[AREA_DEFINE]\n[PLAYERMAPPER_DEFINE]\n\"ME\" 0\n[SCRIPT_KERNEL]\n"
               "SET X = TRUE;\nSET Y = FALSE;\nSET Z = TRUE;\nINITBLOCK(){ MONEY_ADD(\"ME\",7); "
               "}\n[SIGNALS]\n[CAMARAPOS]\n0,0\n";
    r.asset_namespace = "authored/joint";
    r.state.context = sim::ScriptContext::Ready;
    r.state.last_arg = 17;
    r.state.last_work = 23;
    r.state.signals = {0, 1, 0, 1};
    r.state.bindings = {sim::PlayerId{0}};
    sim::TypeRegistry::Builder builder;
    for (auto id : {70001u, 90001u}) {
        sim::BuildingType type;
        type.id = sim::TypeId{id};
        builder.add(type);
    }
    const auto types = builder.build();
    REQUIRE(types);
    auto players = sim::make_player_table(sim::SimConfig{});
    players.slots[0] = p;
    REQUIRE(sim::restore_script(r, *types.value(), players, sim::SimConfig{}).empty());
    REQUIRE(sim::validate_script(r, *types.value(), players, sim::SimConfig{}).empty());
    compare(e.at("script"), r);
    // Direct wrong readings fail the same oracle; a round-trip alone would miss these.
    registry.list.slots[8]->job = 0x2a;
    sim::WriteArchive wrong;
    sim::fields(wrong, registry);
    CHECK(wrong.data() != e.at("figures").bytes);
    p.permissions[1].value = 1;
    sim::WriteArchive normalized;
    sim::fields(normalized, p);
    CHECK(normalized.data() != e.at("player").bytes);
}
