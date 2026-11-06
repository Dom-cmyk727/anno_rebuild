#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/city_accounts.hpp>
#include <anno/sim/goods_metadata.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/test/game_dir.hpp>

#include "command_helpers.hpp"

namespace {
namespace sim = anno::sim;
std::vector<std::uint8_t> bytes(const std::string& s) {
    return {s.begin(), s.end()};
}
std::vector<std::uint8_t> read(const std::filesystem::path& p) {
    std::ifstream file(p, std::ios::binary);
    REQUIRE(file.good());
    return {std::istreambuf_iterator<char>(file), {}};
}
std::uint64_t digest(const std::vector<sim::GoodsInfo>& g) {
    return sim::TypeRegistry::digest_of({}, g, sim::TypeRegistry::empty()->class_table());
}
std::vector<sim::GoodsInfo> goods() {
    return {{"UNUSED"}, {"ALPHA"}, {"BETA"}};
}
std::string parse(const std::string& body, std::vector<sim::GoodsInfo>& g,
                  sim::EconomyConfig economy = {}) {
    return sim::apply_goods_metadata(bytes("[WAREN]\nId: ALPHA\n" + body), g, economy);
}
} // namespace

TEST_CASE("economy facts: ordered signed prices partial taxes and transactional refusals") {
    auto g = goods();
    sim::EconomyConfig roles;
    roles.climate_names = {"ONE", "TWO", "CUSTOM"};
    roles.tax_count = 4;
    REQUIRE(parse("Preis: 45\nBonus: 5, TWO|TWO\n", g, roles).empty());
    REQUIRE(g[1].prices);
    CHECK(g[1].prices->climate == std::vector<std::uint32_t>{45, 55, 45});
    REQUIRE(parse("Preis: 45\nBonus: 5, TWO|TWO\nPreisLevel: 0,5,10,15\n"
                  "PreisLevel: 2\nPreis: 50\nMalus: 60, CUSTOM\nBonus: 20, CUSTOM\n"
                  "PreisLevel:\nBauFirst: INFRA_STUFE_2B\nIdSec: BETA\n",
                  g, roles)
                .empty());
    REQUIRE(g[1].prices);
    CHECK(g[1].prices->climate == std::vector<std::uint32_t>{50, 50, 10});
    CHECK(g[1].prices->tax == std::vector<std::uint32_t>{2, 5, 10, 15});
    CHECK(g[1].bau_first == 5);
    CHECK(g[1].id_sec == 2);
    // Original empty source list is a no-op, including table identity.
    const auto before = digest(g);
    CHECK(parse("PreisLevel:\n", g, roles).empty());
    CHECK(digest(g) == before);
    const std::array<std::pair<const char*, const char*>, 9> invalid{
        {{"Preis: -1\n", "resolved_price"},
         {"PreisLevel: -1\n", "resolved_surcharge"},
         {"PreisLevel: 0,1,2,3,4\n", "columns"},
         {"Bonus: 1, UNKNOWN\n", "climate"},
         {"Malus: -9223372036854775808, ONE\n", "negation"},
         {"Preis: 9223372036854775807\nBonus: 1, ONE\nPreis: 0\n", "accumulator"},
         {"Preis: 9223372036854775808\n", "signed i64"},
         {"Bonus: -9223372036854775809, ONE\n", "signed i64"},
         {"Preis: 4294967296\n", "resolved_price"}}};
    for (const auto& [body, field] : invalid) {
        CAPTURE(body);
        CHECK(parse(std::string("BauFirst: INFRA_EVER\nIdSec: UNUSED\n") + body, g, roles)
                  .find(field) != std::string::npos);
        CHECK(digest(g) == before); // Earlier BauFirst/IdSec changes also roll back.
    }
    CHECK(parse("Preis: -1\nBonus: 2, ONE|TWO|CUSTOM\n", g, roles).empty());
    CHECK(g[1].prices->climate == std::vector<std::uint32_t>{1, 1, 1});
    CHECK(
        parse("Preis: 9223372036854775807\nBonus: -9223372036854775807, ONE|TWO|CUSTOM\n", g, roles)
            .empty());
    CHECK(g[1].prices->climate == std::vector<std::uint32_t>{0, 0, 0});
    CHECK(parse("Preis: -9223372036854775808\nBonus: 9223372036854775807, ONE|TWO|CUSTOM\nBonus: "
                "1, ONE|TWO|CUSTOM\n",
                g, roles)
              .empty());
    CHECK(g[1].prices->climate == std::vector<std::uint32_t>{0, 0, 0});
}

TEST_CASE(
    "economy facts: inert selectors preserve constructor exact-length masks and unknownness") {
    sim::BuildingType t;
    t.cls = sim::RuntimeClass::Hunter;
    CHECK(sim::inert_financial_active(t) == true);
    CHECK(sim::inert_financial_active(t, std::vector<std::uint8_t>{0}) == false);
    CHECK(sim::inert_financial_active(t, std::vector<std::uint8_t>{0, 2}) == true);
    CHECK(sim::inert_financial_active(t, std::vector<std::uint8_t>{0, 4}) == false);
    CHECK_FALSE(sim::inert_financial_active(t, std::vector<std::uint8_t>(15)).has_value());
    t.cls = sim::RuntimeClass::DualWorkshop;
    CHECK(sim::inert_financial_active(t, std::vector<std::uint8_t>{0, 2}) == true);
    t.cls = sim::RuntimeClass::Castle;
    CHECK(sim::inert_financial_active(t, std::vector<std::uint8_t>(10)) == false);
    CHECK(sim::inert_financial_active(t, std::vector<std::uint8_t>(9)) == true);
    t.cls = sim::RuntimeClass::Whaler;
    CHECK(sim::inert_financial_active(t, std::vector<std::uint8_t>{0}) == true);
    CHECK_FALSE(sim::inert_financial_active(t, std::vector<std::uint8_t>{1}).has_value());
    auto whaler = std::vector<std::uint8_t>(11);
    whaler[0] = 1;
    whaler[1] = 1;
    CHECK(sim::inert_financial_active(t, whaler) == false); // outer2, not1
    whaler[2] = 1;
    CHECK(sim::inert_financial_active(t, whaler) == true);
    t.cls = sim::RuntimeClass::MarketStall;
    t.output_goods = {0, 159};
    CHECK(sim::inert_financial_active(t) == true);
    CHECK(sim::inert_financial_active(t, std::vector<std::uint8_t>{1}) == false);
    CHECK(sim::inert_financial_active(t, std::vector<std::uint8_t>{2}) == true);
    CHECK(sim::inert_financial_active(t, std::vector<std::uint8_t>{0, 0}) ==
          true); // skipped length
    t.output_goods = {0, 0};
    CHECK(sim::inert_financial_active(t) == false);
}

TEST_CASE("economy facts: generic dimensions wide tax selector and exact native history") {
    auto t = command_test::build_types();
    auto cfg = sim::SimConfig{};
    cfg.warehouse_goods = 160;
    cfg.billed_goods_first = 149;
    cfg.billed_goods_last = 159;
    cfg.economy.climate_names.push_back("CUSTOM");
    cfg.monopoly_climate_mask.push_back(0);
    cfg.economy.tax_count = 300;
    auto g = sim::original_goods();
    for (std::size_t i = g.size(); i < 160; ++i)
        g.push_back(sim::GoodsInfo{"G" + std::to_string(i)});
    g[159].prices =
        sim::GoodsPrices{std::vector<std::uint32_t>(8, 17), std::vector<std::uint32_t>(300, 3)};
    sim::TypeRegistry::Builder builder;
    builder.config(cfg).goods(g).walk_tables(t.registry->walk_tables());
    for (const auto& type : t.registry->types())
        builder.add(type);
    auto producer = *t.registry->find(t.weaver);
    producer.id = sim::TypeId{70001};
    producer.output_goods = {159};
    producer.prod.outputs = {"G159"};
    producer.running_cost = {2, 14};
    builder.add(producer);
    sim::BuildingType service;
    service.id = sim::TypeId{70002};
    service.prodtype = 67;
    service.cls = sim::RuntimeClass::PublicService;
    service.output_goods = {159};
    service.running_cost = {4, 28};
    service.statistic_kind = 2;
    builder.add(service);
    const auto registry = builder.build();
    REQUIRE_MESSAGE(registry.ok(), (registry ? "" : registry.error()));
    t.registry = registry.value();
    auto spec = command_test::two_city_spec(t);
    for (auto& c : spec.islands[0].cities)
        c.stock->resize(160);
    spec.settings.prop10 = 299;
    sim::Rules rules;
    rules.city_booking = true;
    auto made = sim::World::create(spec, t.registry, cfg, rules);
    REQUIRE_MESSAGE(made.ok(), (made ? "" : made.error()));
    auto w = std::move(made).value();
    auto& c = command_test::city_mut(w, command_test::kCity0);
    c.accounts = {65000, 0x0102030405060708ULL, 105, 210, 4294967313ULL, 28};
    c.billed[159] = 4294967295U;
    const auto saved = w.save();
    auto loaded = sim::World::load(saved, t.registry);
    REQUIRE_MESSAGE(loaded.ok(), (loaded ? "" : loaded.error()));
    CHECK(loaded.value().save() == saved);
    CHECK(loaded.value().state_hash() == w.state_hash());
    CHECK(loaded.value().settings().prop10 == 299);
    const auto& restored = command_test::city(loaded.value(), command_test::kCity0);
    CHECK(restored.accounts.civil_cost == 4294967313ULL);
    CHECK(restored.accounts.sales == 0x0102030405060708ULL);
    CHECK(restored.billed[159] == 4294967295U);
    auto reordered = cfg;
    std::swap(reordered.economy.climate_names[0], reordered.economy.climate_names[7]);
    const auto wrong_roles = sim::World::create(spec, t.registry, reordered, rules);
    REQUIRE_FALSE(wrong_roles);
    CHECK(wrong_roles.error().find("sealed price role binding") != std::string::npos);
    CHECK_FALSE(sim::Rules{}.city_booking);
    rules.save_billed_goods = false;
    CHECK(rules.validate().find("save_billed_goods") != std::string::npos);
    CHECK_FALSE(sim::World::create(spec, t.registry, cfg, rules));
    g[159].prices->tax.pop_back();
    sim::TypeRegistry::Builder malformed;
    malformed.config(cfg).goods(g);
    const auto refused = malformed.build();
    REQUIRE_FALSE(refused);
    CHECK(refused.error().find("prices.climate/tax") != std::string::npos);
    CHECK(w.save() == saved);
    auto invalid_roles = cfg;
    invalid_roles.economy.tax_count = 0;
    sim::TypeRegistry::Builder zero_tax;
    const auto invalid_registry = zero_tax.config(invalid_roles).build();
    REQUIRE_FALSE(invalid_registry);
    CHECK(invalid_registry.error().find("economy.tax_count") != std::string::npos);
    CHECK(w.save() == saved);
}

TEST_CASE("economy facts: actual132 archives refuse before registry or money mutation") {
    const auto t = command_test::build_types();
    auto w = command_test::make_world(t, command_test::two_city_spec(t));
    const auto current = w.save();
    REQUIRE(sim::World::load(current, t.registry));
    const auto path = std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "econbook1" / "legacy132";
    const auto old = sim::World::load(read(path / "g2.save"), t.registry);
    REQUIRE_FALSE(old);
    CHECK(old.error() == "save: state stream version 132 is not supported (this build reads 133)");
    const auto log = sim::read_log(read(path / "g2.log"));
    REQUIRE_FALSE(log);
    CHECK(log.error() == "stream (byte 24): replay state stream version differs from this build's");
    CHECK(w.save() == current);
    auto recorder = sim::Recorder::start(w);
    REQUIRE(recorder);
    auto good = sim::write_log(recorder.value().log());
    REQUIRE(good);
    REQUIRE(sim::read_log(good.value()));
}

TEST_CASE(
    "economy facts: unknown financial input refuses only opted-in world and persists known flags") {
    auto t = command_test::build_types();
    sim::TypeRegistry::Builder builder;
    builder.walk_tables(t.registry->walk_tables());
    for (const auto& type : t.registry->types())
        builder.add(type);
    sim::BuildingType stall;
    stall.id = sim::TypeId{70003};
    stall.prodtype = 161;
    stall.cls = sim::RuntimeClass::MarketStall;
    stall.output_goods = {26, 0};
    stall.running_cost = {2, 10};
    builder.add(stall);
    const auto types = builder.build();
    REQUIRE_MESSAGE(types.ok(), (types ? "" : types.error()));
    t.registry = types.value();
    auto spec = command_test::two_city_spec(t);
    sim::ScenarioSpec::BuildingSpec b;
    b.type = stall.id;
    b.x = 125;
    b.y = 110;
    sim::Inert unknown;
    unknown.financial_active.reset();
    b.payload = unknown;
    spec.islands[0].cities[0].buildings.push_back(b);
    REQUIRE(sim::World::create(spec, t.registry)); // default-off body remains allowed
    sim::Rules rules;
    rules.city_booking = true;
    const auto refused = sim::World::create(spec, t.registry, {}, rules);
    REQUIRE_FALSE(refused);
    CHECK(refused.error().find("financial_active") != std::string::npos);
    unknown.financial_active = false;
    spec.islands[0].cities[0].buildings.back().payload = unknown;
    auto made = sim::World::create(spec, t.registry, {}, rules);
    REQUIRE_MESSAGE(made.ok(), (made ? "" : made.error()));
    auto w = std::move(made).value();
    const auto saved = w.save();
    auto loaded = sim::World::load(saved, t.registry);
    REQUIRE(loaded);
    CHECK(loaded.value().save() == saved);
    const auto* building = loaded.value().building_at(command_test::kCity0, 125, 110);
    REQUIRE(building);
    CHECK(std::get<sim::Inert>(building->payload).financial_active == false);
    sim::WorldTestAccess::rules(w).save_billed_goods = false;
    const auto bad = sim::World::load(w.save(), t.registry);
    REQUIRE_FALSE(bad);
    CHECK(bad.error().find("save_billed_goods") != std::string::npos);
    auto cfg = sim::SimConfig{};
    cfg.economy.climate_names.pop_back();
    CHECK_FALSE(sim::World::create(spec, t.registry, cfg, rules));
    cfg = sim::SimConfig{};
    cfg.economy.period = 0;
    CHECK(cfg.validate().find("economy.period") != std::string::npos);
}

TEST_CASE("economy facts: original registry prices and real financial prerequisites") {
    const auto dir = anno::test::game_dir();
    if (!dir)
        return;
    const auto loaded =
        anno::sim_load::load_original_registry(*dir, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(loaded.types, loaded.error);
    const auto* food = loaded.types->goods(26);
    REQUIRE(food);
    REQUIRE(food->prices);
    CHECK(food->prices->climate == std::vector<std::uint32_t>{45, 45, 45, 50, 50, 50, 45});
    CHECK(food->prices->tax == std::vector<std::uint32_t>{0, 0, 0});
    const auto* alcohol = loaded.types->goods(29);
    REQUIRE(alcohol);
    REQUIRE(alcohol->prices);
    CHECK(alcohol->prices->tax == std::vector<std::uint32_t>{0, 5, 10});
    const auto* hunter = loaded.types->find(sim::TypeId{805});
    REQUIRE(hunter);
    CHECK(hunter->statistic_kind == 1);
    CHECK(sim::inert_financial_active(*hunter, std::vector<std::uint8_t>{0, 3, 0, 0, 0, 0, 0, 0, 0,
                                                                         0, 0, 0, 0, 0}) == true);
}
