// Construction facts, explicit-context query, identity and enabled synthetic integration.
// B0 also preserves the unchanged player-build behavior while production wiring is blocked.

#include <algorithm>
#include <deque>

#include <doctest/doctest.h>

#include <anno/formats/chunk.hpp>
#include <anno/sim/build_availability.hpp>
#include <anno/sim/goods_metadata.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim/world.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/test/game_dir.hpp>

#include "command_helpers.hpp"

namespace sim = anno::sim;
TEST_CASE(
    "bootstrap B0 successor: authoritative locked build and scenario exception (game files)") {
    const auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("SKIP: ANNO1503_GAME_DIR is unset");
        return;
    }
    const auto loaded =
        anno::sim_load::load_original_registry(*game, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(loaded.error.empty(), loaded.error);
    for (const auto& [id, expected] :
         std::vector<std::pair<std::uint32_t, std::vector<std::uint32_t>>>{
             {1006, {4, 9}}, {1012, {12, 38}}, {1341, {49}}, {1021, {35, 36}}}) {
        const auto* actual = loaded.types->find(sim::TypeId{id});
        REQUIRE(actual != nullptr);
        CHECK(actual->construction.research_any == expected);
    }
    CHECK(loaded.types->find(sim::TypeId{1021})->construction.stage == 17);
    const auto* inn = loaded.types->find(sim::TypeId{1335});
    REQUIRE(inn != nullptr);
    for (const auto mask : {1U, 5U}) {
        auto spec = sim::synthetic::empty_scenario(77);
        sim::ScenarioSpec::PlayerSpec p;
        p.id = sim::PlayerId{0};
        p.money = 30000;
        p.flags = mask;
        spec.players.push_back(p);
        auto is = sim::synthetic::plain_island(0, 100, 100, 32, 32);
        is.owner_runs = {{0, 32 * 32}};
        std::vector<std::uint8_t> heights(32 * 32, 0x30);
        is.terrain = sim::make_terrain(heights, std::vector<std::uint16_t>(heights.size()),
                                       std::vector<std::uint8_t>(heights.size()));
        auto city = sim::synthetic::plain_city(0, 0);
        city.stock = std::vector<std::uint32_t>(sim::SimConfig{}.warehouse_goods, 0);
        for (auto g : {41U, 42U, 43U, 44U, 45U})
            (*city.stock)[g] = 50U * 256U;
        sim::ScenarioSpec::BuildingSpec imported;
        imported.type = sim::TypeId{1335};
        imported.x = 102;
        imported.y = 102;
        city.buildings.push_back(imported);
        is.cities.push_back(city);
        spec.islands.push_back(is);
        auto made = sim::World::create(spec, loaded.types);
        REQUIRE_MESSAGE(made.ok(), (made.ok() ? std::string() : made.error()));
        auto world = std::move(made).value();
        REQUIRE(world.building_at(command_test::kCity0, 102, 102) != nullptr);
        const auto result = world.apply(command_test::build(
            0, command_test::kCity0, {command_test::at(sim::TypeId{1335}, 112, 112)}));
        CHECK(result.status == (mask == 1 ? sim::Reject::ConstructionDenied : sim::Reject::None));
    }
}

namespace {
namespace fmt = anno::formats;
fmt::chunk::Chunk rec(const char* name, std::string text, bool terminated = true) {
    static std::deque<std::vector<std::uint8_t>> storage;
    storage.emplace_back(text.begin(), text.end());
    if (terminated) {
        storage.back().push_back(0);
    }
    fmt::chunk::Chunk r;
    r.name_field = *fmt::chunk::name_field(name);
    r.payload = storage.back();
    return r;
}
fmt::figuren::Entry entry(std::uint32_t id, std::string stage = "") {
    fmt::figuren::Entry e;
    e.id = id;
    auto h = rec("HEAD", std::string(2, '\1') + std::string(54, '\0'), false);
    e.records.push_back(h);
    e.records.push_back(rec("PRODTYPE", "PIONIER"));
    if (!stage.empty())
        e.records.push_back(rec("INFRAMIN", stage));
    return e;
}
auto parsed(const std::vector<fmt::figuren::Entry>& entries,
            const sim::BuildAvailabilityProfile& profile = {},
            std::vector<sim::GoodsInfo> goods = sim::original_goods()) {
    return sim::TypeRegistry::build(entries, std::move(goods), {}, {}, {}, {}, {}, false, profile);
}
sim::PlayerTable players() {
    sim::PlayerTable p = sim::make_player_table({});
    p.slots[0] = sim::make_player(sim::PlayerId{0}, {});
    p.slots[1] = sim::make_player(sim::PlayerId{1}, {});
    return p;
}
} // namespace
TEST_CASE("bootstrap facts: defaults boundaries invalid content and alternate ids") {
    std::vector<fmt::figuren::Entry> es{entry(90001), entry(90002, "INFRA_EVER"),
                                        entry(90003, "INFRA_STUFE_FORSCH")};
    const auto r = parsed(es);
    REQUIRE(r.ok());
    CHECK(r.value()->find(sim::TypeId{90001})->construction.stage == 0);
    CHECK(r.value()->find(sim::TypeId{90002})->construction.stage == 0);
    CHECK(r.value()->find(sim::TypeId{90003})->construction.stage == 31);
    CHECK(r.value()->find(sim::TypeId{90003})->construction.research_any ==
          std::vector<std::uint32_t>{0});
    for (const auto name : {"INFRA_STUFE_9Z", "GOTT", "SPIELER", "UNUSED"}) {
        auto bad = parsed({entry(90001, name)});
        REQUIRE_FALSE(bad.ok());
        CHECK(bad.error().find("90001") != std::string::npos);
        CHECK(bad.error().find("INFRAMIN") != std::string::npos);
    }
    auto bad = entry(90001, "INFRA_EVER");
    bad.records.push_back(rec("INFRAMIN", "INFRA_STUFE_1C"));
    CHECK_FALSE(parsed({bad}).ok());
    bad = entry(90001, "INFRA_EVER");
    bad.records.back().payload =
        bad.records.back().payload.first(bad.records.back().payload.size() - 1);
    CHECK_FALSE(parsed({bad}).ok());
    bad = entry(90001, "INFRA_EVER");
    bad.records.back() = rec("INFRAMIN", std::string("INFRA_EVER\0X", 12));
    CHECK_FALSE(parsed({bad}).ok());
    auto t = *r.value()->find(sim::TypeId{90001});
    sim::TypeRegistry::Builder b;
    t.construction.stage = 32;
    b.add(t);
    CHECK_FALSE(b.build().ok());
    sim::TypeRegistry::Builder b2;
    t.construction.stage = 3;
    t.construction.research_any = {65};
    b2.add(t);
    CHECK_FALSE(b2.build().ok());
    CHECK_FALSE(parsed(es, {{9999}, {}}).ok());
    CHECK_FALSE(parsed(es, {{}, {{sim::TypeId{9999}, {1}}}}).ok());
    CHECK_FALSE(parsed(es, {{}, {{sim::TypeId{90001}, {1}}, {sim::TypeId{90001}, {2}}}}).ok());
}
TEST_CASE("bootstrap query: bit owner research and scenario permission discrimination") {
    const auto reg =
        parsed({entry(90001, "INFRA_STUFE_1C"), entry(90002), entry(90003, "INFRA_STUFE_FORSCH")});
    REQUIRE(reg.ok());
    auto p = players();
    auto& a = *p.slots[0];
    a.flags = 8;
    p.slots[1]->flags = 16;
    auto q = [&](std::uint32_t player, std::uint32_t type) {
        return sim::build_eligibility(*reg.value(), p, sim::PlayerId{player}, sim::TypeId{type});
    };
    CHECK(q(0, 90001).allowed());
    CHECK_FALSE(q(1, 90001).allowed());
    CHECK(q(1, 90001).status == sim::BuildEligibilityStatus::StageLocked);
    a.flags = 0;
    CHECK_FALSE(q(0, 90002).allowed());
    a.flags = 1;
    CHECK(q(0, 90002).allowed());
    a.flags = 0x80000000;
    CHECK(q(0, 90003).allowed());
    a.flags = 1;
    a.research.state[0] = 1;
    CHECK_FALSE(q(0, 90003).allowed());
    a.research.state[0] = 2;
    CHECK(q(0, 90003).allowed());
    CHECK_FALSE(q(0, 90001).allowed());
    const std::vector<sim::BuildPermission> deny{{sim::TypeId{90002}, 0}},
        allow{{sim::TypeId{90001}, 2}};
    const auto query = [&](auto rows, sim::PlayerId owner = sim::PlayerId{0}) {
        return sim::build_eligibility(*reg.value(), p, sim::PlayerId{0}, rows[0].type,
                                      {owner, rows});
    };
    CHECK(query(std::span(deny)).status == sim::BuildEligibilityStatus::ScenarioDenied);
    CHECK(query(std::span(allow)).allowed());
    CHECK(query(std::span(allow)).value == 2);
    CHECK(query(std::span(allow), sim::PlayerId{1}).status ==
          sim::BuildEligibilityStatus::InvalidPermissions);
    CHECK(q(2, 90001).status == sim::BuildEligibilityStatus::UnknownPlayer);
    CHECK(q(0, 999).status == sim::BuildEligibilityStatus::UnknownType);
    const std::vector<sim::BuildPermission> repeated{{sim::TypeId{90001}, 2},
                                                     {sim::TypeId{90001}, 0}};
    CHECK(query(std::span(repeated)).status == sim::BuildEligibilityStatus::InvalidPermissions);
    const std::vector<sim::BuildPermission> malformed{{sim::TypeId{90001}, 256}};
    CHECK(query(std::span(malformed)).status == sim::BuildEligibilityStatus::InvalidPermissions);
}
TEST_CASE("bootstrap facts: research output guard and data profile alternatives") {
    auto goods = sim::original_goods();
    goods.resize(202);
    goods[200].name = "ALPHA";
    goods[200].forsch = 7;
    goods[201].name = "BETA";
    goods[201].forsch = 9;
    auto e = entry(90001, "INFRA_STUFE_FORSCH");
    e.records.push_back(rec("WARE", std::string("ALPHA\0BETA", 10)));
    auto other = entry(90002, "INFRA_STUFE_FORSCH");
    other.records.push_back(rec("WARE", std::string("BETA\0ALPHA", 10)));
    auto ordinary = entry(90003, "INFRA_STUFE_1C");
    ordinary.records.push_back(rec("FORSCHTYPE", "BRUNNEN"));
    auto extra = entry(90004, "INFRA_STUFE_4A");
    auto researched_type = entry(90005, "INFRA_STUFE_FORSCH");
    researched_type.records.push_back(rec("FORSCHTYPE", "BRUNNEN"));
    const sim::BuildAvailabilityProfile profile{{200}, {{sim::TypeId{90004}, {35, 36}}}};
    const auto r = parsed({e, other, ordinary, extra, researched_type}, profile, goods);
    REQUIRE(r.ok());
    CHECK(r.value()->find(sim::TypeId{90001})->construction.research_any ==
          std::vector<std::uint32_t>{0, 7, 9});
    CHECK(r.value()->find(sim::TypeId{90002})->construction.research_any ==
          std::vector<std::uint32_t>{0});
    CHECK(r.value()->find(sim::TypeId{90003})->construction.research_any.empty());
    auto p = players();
    CHECK_FALSE(
        sim::build_eligibility(*r.value(), p, sim::PlayerId{0}, sim::TypeId{90005}).allowed());
    p.slots[0]->research.state[49] = 1;
    CHECK_FALSE(
        sim::build_eligibility(*r.value(), p, sim::PlayerId{0}, sim::TypeId{90005}).allowed());
    p.slots[0]->research.state[49] = 2;
    CHECK(sim::build_eligibility(*r.value(), p, sim::PlayerId{0}, sim::TypeId{90005}).allowed());
    p.slots[0]->research.state[9] = 2;
    CHECK(sim::build_eligibility(*r.value(), p, sim::PlayerId{0}, sim::TypeId{90001}).allowed());
    CHECK_FALSE(
        sim::build_eligibility(*r.value(), p, sim::PlayerId{0}, sim::TypeId{90002}).allowed());
    p.slots[0]->research.state[49] = 2;
    CHECK_FALSE(
        sim::build_eligibility(*r.value(), p, sim::PlayerId{0}, sim::TypeId{90003}).allowed());
    p.slots[0]->research.state[36] = 2;
    CHECK(sim::build_eligibility(*r.value(), p, sim::PlayerId{0}, sim::TypeId{90004}).allowed());
    CHECK_FALSE(
        sim::build_eligibility(*r.value(), p, sim::PlayerId{1}, sim::TypeId{90004}).allowed());
    // The runtime stage/research domain is validated again when a world uses another config.
    sim::SimConfig narrow;
    narrow.research_entries = 8;
    CHECK_FALSE(r.value()->validate_against(narrow).empty());
}
TEST_CASE("bootstrap identity: independent extension bytes preserve default registry") {
    auto e = entry(90001);
    const auto base = parsed({e});
    REQUIRE(base.ok());
    e.records.push_back(rec("INFRAMIN", "INFRA_STUFE_1C"));
    const auto changed = parsed({e});
    REQUIRE(changed.ok());
    // Independently hand-transcribed BVA1,u64 count1,u32 type90001,u32 stage3,u64 research-count0.
    const std::vector<std::uint8_t> extension = {0x42, 0x56, 0x41, 0x31, 1, 0, 0, 0, 0, 0,
                                                 0,    0,    0x91, 0x5f, 1, 0, 3, 0, 0, 0,
                                                 0,    0,    0,    0,    0, 0, 0, 0};
    std::uint64_t expected = base.value()->digest();
    for (auto byte : extension) {
        expected ^= byte;
        expected *= 0x100000001b3ULL;
    }
    CHECK(changed.value()->digest() == expected);
    auto explicit_zero = parsed({entry(90001, "INFRA_EVER")});
    REQUIRE(explicit_zero.ok());
    CHECK(base.value()->digest() == explicit_zero.value()->digest());
    auto extra = parsed({e}, {{}, {{sim::TypeId{90001}, {35}}}});
    REQUIRE(extra.ok());
    CHECK(extra.value()->digest() != changed.value()->digest());
}

TEST_CASE("bootstrap integration: earned synthetic unlock query build debit reload replay") {
    // Synthetic content integration, not authoritative refusal or a ship-start progression claim.
    auto base = sim::synthetic::make_population_types();
    REQUIRE(base.ok());
    auto t = base.value();
    const auto cfg = sim::synthetic::population_config();
    sim::TypeRegistry::Builder builder;
    builder.config(cfg).walk_tables(sim::synthetic_walk_tables());
    builder.house_groups(std::vector<sim::HouseGroup>(t.registry->house_groups().begin(),
                                                      t.registry->house_groups().end()));
    builder.stage_table({{1, 0, 0, 5}});
    for (auto type : t.registry->types()) {
        if (type.id == t.weaver) {
            type.construction.stage = 1;
            type.flags_word = 0x84; // ordinary land fixture, as the original workshop class
            type.cost_gold = 300;
            type.cost_goods = {{41, 768}, {42, 1024}};
        }
        builder.add(type);
    }
    const auto registry = builder.build();
    REQUIRE(registry.ok());
    t.registry = registry.value();
    auto spec = sim::synthetic::g7_scenario(t);
    std::vector<std::uint8_t> height(spec.islands[0].w * spec.islands[0].h, 0x30);
    spec.islands[0].terrain = sim::make_terrain(height, std::vector<std::uint16_t>(height.size()),
                                                std::vector<std::uint8_t>(height.size()));
    sim::Rules rules;
    rules.house_updates = true; // explicitly enabled fixture only
    auto made = sim::World::create(spec, t.registry, cfg, rules);
    REQUIRE_MESSAGE(made.ok(), (made.ok() ? std::string() : made.error()));
    auto w = std::move(made).value();
    auto started = sim::Recorder::start(w);
    REQUIRE(started.ok());
    auto recorder = std::move(started).value();
    const auto query = [&](const sim::World& world) {
        return sim::build_eligibility(world.types(), world.players(), sim::PlayerId{0}, t.weaver);
    };
    const auto before = w.save();
    CHECK_FALSE(query(w).allowed());
    CHECK(w.save() == before);
    // No player flags are injected: the existing population stage pass earns the bit.
    for (unsigned i = 0; i < 400 && !query(w).allowed(); ++i)
        recorder.advance(20);
    REQUIRE(query(w).allowed());
    REQUIRE(w.players().find(sim::PlayerId{0})->flags == 3);
    const auto saved = w.save();
    auto loaded = sim::World::load(saved, t.registry, {});
    REQUIRE(loaded.ok());
    CHECK(query(loaded.value()).allowed());
    CHECK(loaded.value().save() == saved);
    const auto* type = t.registry->find(t.weaver);
    REQUIRE(type != nullptr);
    const auto money = w.players().find(sim::PlayerId{0})->money;
    const auto stock = command_test::city(w, command_test::kCity0).stock.stock;
    const auto result = recorder.submit(
        command_test::build(0, command_test::kCity0, {command_test::at(t.weaver, 120, 120)}));
    REQUIRE_MESSAGE(result.ok(), sim::reject_name(result.status));
    CHECK(w.players().find(sim::PlayerId{0})->money == money - 300);
    CHECK(command_test::city(w, command_test::kCity0).stock.stock[41] == stock[41] - 768);
    CHECK(command_test::city(w, command_test::kCity0).stock.stock[42] == stock[42] - 1024);
    for (const auto& cost : type->cost_goods)
        CHECK(command_test::city(w, command_test::kCity0).stock.stock[cost.goods] ==
              stock[cost.goods] - cost.amount_88);
    recorder.checkpoint();
    auto fresh = sim::World::load(recorder.log().initial_save, t.registry, {});
    REQUIRE(fresh.ok());
    auto outcome = sim::replay(fresh.value(), recorder.log());
    REQUIRE(outcome.ok());
    CHECK(outcome.value().reproduced());
    CHECK(fresh.value().save() == w.save());
    // A different immutable requirement gives a different identity and refuses the old save.
    sim::TypeRegistry::Builder different;
    different.config(cfg).walk_tables(sim::synthetic_walk_tables());
    different.house_groups(std::vector<sim::HouseGroup>(t.registry->house_groups().begin(),
                                                        t.registry->house_groups().end()));
    different.stage_table({{1, 0, 0, 5}});
    for (auto other : t.registry->types()) {
        if (other.id == t.weaver)
            other.construction.stage = 2;
        different.add(other);
    }
    auto other = different.build();
    REQUIRE(other.ok());
    CHECK_FALSE(sim::World::load(saved, other.value(), {}).ok());
}
