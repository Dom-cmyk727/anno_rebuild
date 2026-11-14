// Independent populated field bytes and hierarchical digests, authored before comparing C++.
#include <algorithm>
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
    std::ifstream in(std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "current" /
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
    REQUIRE(out.size() == 10);
    return out;
}
template <class T> void compare(const char* name, const Expected& e, const T& value) {
    sim::WriteArchive bytes;
    sim::fields(bytes, value);
    CAPTURE(std::string{name});
    CAPTURE(bytes.data().size());
    CAPTURE(e.bytes.size());
    const auto mismatch =
        std::mismatch(bytes.data().begin(), bytes.data().end(), e.bytes.begin(), e.bytes.end());
    CAPTURE(std::distance(bytes.data().begin(), mismatch.first));
    CHECK(bytes.data() == e.bytes);
    sim::HashArchive hash;
    sim::fields(hash, value);
    CHECK(hash.finish() == e.hash);
}
struct CombinedWorldScope {
    sim::FigureRegistry figures;
    sim::Building mine;
    sim::Building quarry;
};
template <class A, class S> void combined_world_fields(A& a, S& s) {
    a.scope("figures", [&] { sim::fields(a, s.figures); });
    a.scope("mine_building", [&] { sim::fields(a, s.mine); });
    a.scope("quarry_building", [&] { sim::fields(a, s.quarry); });
}
void compare_combined_world(const Expected& e, const CombinedWorldScope& value) {
    sim::WriteArchive bytes;
    combined_world_fields(bytes, value);
    CHECK(bytes.data() == e.bytes);
    sim::HashArchive hash;
    combined_world_fields(hash, value);
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
TEST_CASE("integration oracle: populated freight quarry mason and two-reserver mine scopes") {
    const auto e = expected();
    compare("header", e.at("header"), sim::StateHeader{});
    compare("accounts", e.at("accounts"),
            sim::CityAccounts{65000, 0x0102030405060708ULL, 105, 210, 4294967313ULL, 28});
    auto economy = sim::EconomyConfig{};
    economy.climate_names.push_back("CUSTOM");
    economy.tax_count = 4;
    compare("economy", e.at("economy"), economy);
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
    compare("player", e.at("player"), p);
    sim::FigureRegistry registry;
    registry.list.slots.resize(40);
    registry.list.hint = 8;
    registry.list.cursor = 9;
    registry.stamp = 19;
    auto ship = actor(1, 1503, {}, {}, false, true);
    ship.nav->order = sim::NavOrder{55, true, 2048, 2304};
    ship.nav->ticket = 9;
    ship.nav->seg = {sim::NavSegKind::Turn, 2, 92, 10, 500};
    ship.freight_generation = 4;
    auto& freight = ship.freight.emplace();
    freight.stops = {
        {sim::BuildingId{77}, 2048, 2304, {{sim::FreightDirection::UnloadAmount, 9, 3}}}};
    freight.phase = sim::FreightPhase::Sailing;
    freight.expected_order_id = 55;
    freight.order_generation = 4;
    freight.last_tick = 1000;
    freight.credit_ticks = 33;
    freight.wait_remaining = 7;
    freight.scan_next = 1;
    freight.matching_raw = 512;
    freight.remaining_raw = 256;
    freight.matching_holds = {0};
    freight.empty_holds = {1};
    freight.candidate_next = 1;
    freight.completed_cycles = 2;
    registry.list.slots[37] = std::move(ship);
    registry.list.slots[2] = actor(2, 717, sim::FigureId{1}, 0);
    registry.list.slots[7] = actor(3, 718, sim::FigureId{2});
    registry.list.slots[8] = actor(4, 106, {}, {}, true);
    auto mason = actor(5, 400);
    mason.home = sim::BuildingId{88};
    mason.job = 0x2c;
    mason.harvest = sim::HarvestState{};
    mason.harvest->step = sim::HarvestStep::Cut;
    mason.harvest->start_delay = 0;
    mason.harvest->wait = 4321;
    mason.harvest->target = {113, 111};
    mason.harvest->goal_col = 2;
    mason.harvest->goal_row = 1;
    registry.list.slots[9] = std::move(mason);
    for (std::size_t slot = 10; slot <= 11; ++slot) {
        auto reserver = actor(slot - 4, 400);
        reserver.job = 0x34;
        reserver.transport.target = sim::BuildingId{77};
        reserver.transport.reserved_88 = 256;
        registry.list.slots[slot] = std::move(reserver);
    }
    compare("figures", e.at("figures"), registry);
    sim::Mine mine;
    mine.last = 12;
    mine.switched_on = true;
    mine.deep = false;
    mine.out = 128;
    mine.activity = 3;
    mine.reserved = 512;
    mine.deposit = 3;
    mine.status = {sim::Status::MissingInput, 2, false, 9};
    compare("mine", e.at("mine"), mine);
    CHECK(registry.list.slots[10]->transport.reserved_88 +
              registry.list.slots[11]->transport.reserved_88 ==
          mine.reserved);
    sim::Quarry quarry;
    quarry.last = 22;
    quarry.suspended = true;
    quarry.status = {sim::Status::StorageFull, 3, false, 44};
    quarry.claims = {1, 0};
    compare("quarry", e.at("quarry"), quarry);
    CombinedWorldScope combined;
    combined.figures = registry;
    combined.mine.id = sim::BuildingId{77};
    combined.mine.type = sim::TypeId{70001};
    combined.mine.x = 10;
    combined.mine.y = 11;
    combined.mine.payload = mine;
    combined.quarry.id = sim::BuildingId{88};
    combined.quarry.type = sim::TypeId{90001};
    combined.quarry.x = 20;
    combined.quarry.y = 21;
    combined.quarry.payload = quarry;
    compare_combined_world(e.at("world"), combined);

    // One archive owns every relationship: read it, verify the links, and write it back exactly.
    sim::WriteArchive encoded;
    combined_world_fields(encoded, combined);
    sim::ReadArchive reader(encoded.data());
    CombinedWorldScope restored;
    combined_world_fields(reader, restored);
    REQUIRE_FALSE(reader.failed());
    CHECK(reader.remaining() == 0);
    sim::WriteArchive reencoded;
    combined_world_fields(reencoded, restored);
    CHECK(reencoded.data() == encoded.data());
    REQUIRE(std::holds_alternative<sim::Mine>(restored.mine.payload));
    REQUIRE(std::holds_alternative<sim::Quarry>(restored.quarry.payload));
    const auto& restored_mine = std::get<sim::Mine>(restored.mine.payload);
    const auto& restored_quarry = std::get<sim::Quarry>(restored.quarry.payload);
    CHECK(restored.mine.id == sim::BuildingId{77});
    CHECK(restored.quarry.id == sim::BuildingId{88});
    CHECK(restored_mine.deposit == 3);
    CHECK(restored_quarry.claims == std::vector<std::uint8_t>{1, 0});
    REQUIRE(restored.figures.list.slots[37]->freight.has_value());
    CHECK(restored.figures.list.slots[37]->freight->stops[0].warehouse == restored.mine.id);
    CHECK(restored.figures.list.slots[9]->home == restored.quarry.id);
    std::uint32_t restored_reservations = 0;
    for (std::size_t slot : {10U, 11U}) {
        REQUIRE(restored.figures.list.slots[slot].has_value());
        CHECK(restored.figures.list.slots[slot]->transport.target == restored.mine.id);
        restored_reservations += restored.figures.list.slots[slot]->transport.reserved_88;
    }
    CHECK(restored_reservations == restored_mine.reserved);

    // Seam omissions are visible in the encompassing bytes, not hidden by isolated integers.
    auto wrong_quarry = restored;
    wrong_quarry.quarry.payload = sim::Inert{}; // omits payload tag 9 and the claim set
    sim::WriteArchive without_quarry_case;
    combined_world_fields(without_quarry_case, wrong_quarry);
    CHECK(without_quarry_case.data() != e.at("world").bytes);
    auto without_route = restored;
    without_route.figures.list.slots[37]->freight.reset();
    sim::WriteArchive without_freight_state;
    combined_world_fields(without_freight_state, without_route);
    CHECK(without_freight_state.data() != e.at("world").bytes);
    auto without_claim = restored;
    std::get<sim::Quarry>(without_claim.quarry.payload).claims[0] = 0;
    sim::WriteArchive without_restored_claim;
    combined_world_fields(without_restored_claim, without_claim);
    CHECK(without_restored_claim.data() != e.at("world").bytes);
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
    compare("script", e.at("script"), r);
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
