// Stage S8: World::apply for SetSpeed and SetProductionLine, the rejection rules, the AI
// seam's commands inside advance, and the closed World (docs/sim/state.md §8.2, §8.4).
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/draw.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/tick.hpp>
#include <anno/sim/world.hpp>

#include "alloc_probe.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;

namespace {

sim::ScenarioSpec::BuildingSpec place(sim::TypeId type, std::int32_t x, std::int32_t y) {
    sim::ScenarioSpec::BuildingSpec b;
    b.type = type;
    b.x = x;
    b.y = y;
    return b;
}

/// Two players, island 0 (city 0 of player 0: weaver, farm, bow maker, small farm, iron
/// mine, kontor) and island 3 (city 1 of player 1: a weaver). The weaver of city 0 has 5 t
/// of wool.
struct Rig {
    syn::SyntheticTypes types;
    sim::World world;
    // The tiles of the buildings.
    static constexpr sim::TileXY weaver{110, 110}, farm{120, 110}, bows{112, 110},
        small_farm{114, 110}, mine{110, 118}, kontor{116, 110}, other_weaver{310, 205};
};

sim::ScenarioSpec rig_scenario(const syn::SyntheticTypes& t) {
    sim::ScenarioSpec spec = syn::two_island_scenario();
    auto weaver = place(t.weaver, Rig::weaver.x, Rig::weaver.y);
    {
        sim::Workshop w = sim::make_workshop(t.registry->find(t.weaver)->prod);
        w.in[0] = 5 * sim::kTon;
        weaver.payload = w;
    }
    spec.islands[0].cities[0].buildings = {
        weaver,
        place(t.farm, Rig::farm.x, Rig::farm.y),
        place(t.bow_maker, Rig::bows.x, Rig::bows.y),
        place(t.small_farm, Rig::small_farm.x, Rig::small_farm.y),
        place(t.iron_mine, Rig::mine.x, Rig::mine.y),
        place(t.kontor, Rig::kontor.x, Rig::kontor.y),
    };
    spec.islands[1].cities[0].buildings = {
        place(t.weaver, Rig::other_weaver.x, Rig::other_weaver.y)};
    return spec;
}

Rig make_rig(sim::Seams seams = {}, sim::SimConfig cfg = {}) {
    auto t = syn::make_synthetic_types();
    REQUIRE_MESSAGE(t.ok(), (t.ok() ? std::string() : t.error()));
    auto w = sim::World::create(rig_scenario(t.value()), t.value().registry, std::move(cfg), {},
                                std::move(seams));
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    return Rig{std::move(t).value(), std::move(w).value()};
}

const sim::Building& building(const sim::World& w, sim::TileXY t, std::uint32_t island = 0,
                              std::uint32_t city = 0) {
    const sim::Building* b =
        w.building_at(sim::CityRef{sim::IslandId{island}, sim::CityNo{city}}, t.x, t.y);
    REQUIRE(b != nullptr);
    return *b;
}

template <class P> const P& payload(const sim::World& w, sim::TileXY t) {
    const auto* p = std::get_if<P>(&building(w, t).payload);
    REQUIRE(p != nullptr);
    return *p;
}

sim::Command line(std::uint32_t issuer, sim::TileXY tile, sim::GoodsId goods, bool on) {
    return sim::Command{sim::PlayerId{issuer}, sim::SetProductionLineCmd{tile, goods, on}};
}

sim::Command speed(std::uint32_t issuer, std::uint32_t percent) {
    return sim::Command{sim::PlayerId{issuer}, sim::SetSpeedCmd{percent}};
}

sim::GoodsId output(const Rig& r, sim::TypeId type, std::size_t i = 0) {
    return r.types.registry->find(type)->output_goods.at(i);
}

// Variable templates: a requires-expression that is ill-formed is only a `false` inside a
// template.
template <class W> constexpr bool can_write_clock = requires(W& w) { w.clock().ms = 1; };
template <class W> constexpr bool can_draw = requires(W& w) { w.rng().next(); };
template <class W> constexpr bool can_seek = requires(W& w) { w.rng().set_cursor(1); };
template <class W>
constexpr bool can_read = requires(const W& w) {
    w.clock().tick();
    w.rng().cursor();
};

} // namespace

TEST_CASE("the world is closed: the clock and the PRNG have no public writer") {
    // S1 can be driven only through apply and advance: the clock and the PRNG are read
    // through const accessors, there is no member to assign to.
    static_assert(!can_write_clock<sim::World>);
    static_assert(!can_draw<sim::World>);
    static_assert(!can_seek<sim::World>);
    static_assert(can_read<sim::World>);
    static_assert(std::is_same_v<decltype(std::declval<const sim::World&>().clock()),
                                 const sim::WorldClock&>);
    static_assert(
        std::is_same_v<decltype(std::declval<const sim::World&>().rng()), const sim::WorldRng&>);
    sim::World w(77);
    CHECK(w.clock().ms == 0);
    CHECK(w.rng().cursor() == 77);
    w.advance(10);
    CHECK(w.clock().ms == 10);
    CHECK(w.clock().last_tick == 5);
    // The test door reaches the same objects.
    CHECK(&sim::WorldTestAccess::clock(w) == &w.clock());
    CHECK(&sim::WorldTestAccess::rng(w) == &w.rng());
}

TEST_CASE("set speed stores the percent for the host") {
    Rig r = make_rig();
    sim::World& w = r.world;
    CHECK(w.settings().speed_percent == 100);
    const std::uint64_t before = w.state_hash();
    for (const std::uint32_t percent : {200U, 50U, 0U, 400U, 100U}) {
        const sim::ApplyResult res = w.apply(speed(0, percent));
        CHECK(res.status == sim::Reject::None);
        CHECK(res.ok());
        CHECK(res.records.empty());
        CHECK(w.settings().speed_percent == percent);
    }
    CHECK(w.state_hash() == before); // back at 100
    w.apply(speed(0, 200));
    CHECK(w.state_hash() != before); // the speed is hashed state
    CHECK(w.check_invariants() == "");
}

TEST_CASE("set speed host only") {
    Rig r = make_rig();
    sim::World& w = r.world;
    const std::uint64_t before = w.state_hash();
    // Player 1 exists but is not the host.
    CHECK(w.apply(speed(1, 200)).status == sim::Reject::NotHost);
    CHECK(w.settings().speed_percent == 100);
    CHECK(w.state_hash() == before);
    // The host is configuration: with host_player 1 the roles swap.
    sim::SimConfig cfg;
    cfg.host_player = sim::PlayerId{1};
    Rig r2 = make_rig({}, cfg);
    CHECK(r2.world.apply(speed(0, 200)).status == sim::Reject::NotHost);
    CHECK(r2.world.apply(speed(1, 200)).ok());
    CHECK(r2.world.settings().speed_percent == 200);
}

TEST_CASE("set speed accepts the original speeds only") {
    Rig r = make_rig();
    sim::World& w = r.world;
    const std::uint64_t before = w.state_hash();
    for (const std::uint32_t bad : {1U, 99U, 101U, 150U, 300U, 401U, 1000U, 0xFFFFFFFFU}) {
        CHECK(w.apply(speed(0, bad)).status == sim::Reject::BadSpeed);
    }
    CHECK(w.state_hash() == before);
    // 400 % and pause are single-player only (is_original_speed): refused in multiplayer.
    sim::SimConfig mp;
    mp.multiplayer = true;
    Rig r2 = make_rig({}, mp);
    CHECK(r2.world.apply(speed(0, 400)).status == sim::Reject::BadSpeed);
    CHECK(r2.world.apply(speed(0, 0)).status == sim::Reject::BadSpeed);
    CHECK(r2.world.apply(speed(0, 50)).ok());
    CHECK(r2.world.apply(speed(0, 200)).ok());
    CHECK(r2.world.settings().speed_percent == 200);
    // Not the host is checked before the value.
    CHECK(r2.world.apply(speed(1, 123)).status == sim::Reject::NotHost);
}

TEST_CASE("unknown issuers are rejected before anything else") {
    Rig r = make_rig();
    sim::World& w = r.world;
    const std::uint64_t before = w.state_hash();
    // Slot 2 is empty, slot 31 is the neutral player's slot (no player in this scenario),
    // 32 and above are beyond the table, the default id is invalid.
    for (const std::uint32_t id : {2U, 31U, 32U, 1000U, 0xFFFFFFFEU, 0xFFFFFFFFU}) {
        CHECK(w.apply(speed(id, 100)).status == sim::Reject::UnknownIssuer);
        CHECK(w.apply(line(id, Rig::weaver, output(r, r.types.weaver), false)).status ==
              sim::Reject::UnknownIssuer);
        CHECK(w.apply(sim::Command{sim::PlayerId{id}, sim::LegacyRawCmd{0x6a, {}}}).status ==
              sim::Reject::UnknownIssuer);
    }
    CHECK(w.apply(sim::Command{sim::PlayerId{}, sim::SetSpeedCmd{100}}).status ==
          sim::Reject::UnknownIssuer);
    CHECK(w.state_hash() == before);
}

TEST_CASE("set production line changes output") {
    Rig r = make_rig();
    sim::World off = r.world;
    sim::World on = r.world;
    const sim::GoodsId goods = output(r, r.types.weaver);
    REQUIRE(payload<sim::Workshop>(off, Rig::weaver).switched_on);
    const std::uint64_t hash_before = off.state_hash();

    const sim::ApplyResult res = off.apply(line(0, Rig::weaver, goods, false));
    CHECK(res.status == sim::Reject::None);
    CHECK(!payload<sim::Workshop>(off, Rig::weaver).switched_on);
    CHECK(off.state_hash() != hash_before);
    CHECK(off.check_invariants() == "");

    // 10 s of game time is 20 production steps for the switched-on weaver (one per 500
    // ticks); the switched-off one does nothing.
    for (int i = 0; i < 1000; ++i) {
        off.advance(10);
        on.advance(10);
    }
    CHECK(payload<sim::Workshop>(on, Rig::weaver).out[0] > 0);
    CHECK(payload<sim::Workshop>(off, Rig::weaver).out[0] == 0);
    CHECK(payload<sim::Workshop>(off, Rig::weaver).in[0] == 5 * sim::kTon);

    // Switch it back on: from then on it produces.
    CHECK(off.apply(line(0, Rig::weaver, goods, true)).ok());
    CHECK(payload<sim::Workshop>(off, Rig::weaver).switched_on);
    for (int i = 0; i < 1000; ++i) {
        off.advance(10);
    }
    CHECK(payload<sim::Workshop>(off, Rig::weaver).out[0] > 0);
    // The command is idempotent: the same switch twice leaves the same state.
    const std::uint64_t h = off.state_hash();
    CHECK(off.apply(line(0, Rig::weaver, goods, true)).ok());
    CHECK(off.state_hash() == h);
    // Only the weaver changed in the other world: the farm of the same city is untouched.
    CHECK(payload<sim::Farm>(on, Rig::farm).switched_on);
}

TEST_CASE("set production line addresses the switch of every class") {
    Rig r = make_rig();
    sim::World& w = r.world;
    // Farm and mine: one switch.
    CHECK(w.apply(line(0, Rig::farm, output(r, r.types.farm), false)).ok());
    CHECK(!payload<sim::Farm>(w, Rig::farm).switched_on);
    CHECK(w.apply(line(0, Rig::mine, output(r, r.types.iron_mine), false)).ok());
    CHECK(!payload<sim::Mine>(w, Rig::mine).switched_on);
    // The weapons workshop has a switch per line, picked by the goods (output index).
    const sim::GoodsId bow = output(r, r.types.bow_maker, 0);
    const sim::GoodsId crossbow = output(r, r.types.bow_maker, 1);
    REQUIRE(bow != crossbow);
    CHECK(payload<sim::WeaponsWorkshop>(w, Rig::bows).line_on == std::vector<bool>{true, true});
    CHECK(w.apply(line(0, Rig::bows, crossbow, false)).ok());
    CHECK(payload<sim::WeaponsWorkshop>(w, Rig::bows).line_on == std::vector<bool>{true, false});
    CHECK(w.apply(line(0, Rig::bows, bow, false)).ok());
    CHECK(payload<sim::WeaponsWorkshop>(w, Rig::bows).line_on == std::vector<bool>{false, false});
    CHECK(w.apply(line(0, Rig::bows, crossbow, true)).ok());
    CHECK(payload<sim::WeaponsWorkshop>(w, Rig::bows).line_on == std::vector<bool>{false, true});
    // The small farm likewise.
    const sim::GoodsId g0 = output(r, r.types.small_farm, 0);
    const sim::GoodsId g1 = output(r, r.types.small_farm, 1);
    REQUIRE(g0 != g1);
    CHECK(w.apply(line(0, Rig::small_farm, g1, false)).ok());
    CHECK(payload<sim::SmallFarm>(w, Rig::small_farm).line_on == std::vector<bool>{true, false});
    CHECK(w.apply(line(0, Rig::small_farm, g0, false)).ok());
    CHECK(payload<sim::SmallFarm>(w, Rig::small_farm).line_on == std::vector<bool>{false, false});
    CHECK(w.check_invariants() == "");
    // The save keeps the switches.
    auto loaded = sim::World::load(w.save(), r.types.registry);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    CHECK(loaded.value().state_hash() == w.state_hash());
}

TEST_CASE("line not found") {
    Rig r = make_rig();
    sim::World& w = r.world;
    const std::uint64_t before = w.state_hash();
    const sim::GoodsId stoffe = output(r, r.types.weaver);
    const sim::GoodsId wool = r.types.registry->find(r.types.weaver)->input_goods.at(0);
    REQUIRE(stoffe != wool);
    // Goods the building does not make: its input, another goods, nonsense.
    CHECK(w.apply(line(0, Rig::weaver, wool, false)).status == sim::Reject::LineNotFound);
    CHECK(w.apply(line(0, Rig::weaver, output(r, r.types.farm), false)).status ==
          sim::Reject::LineNotFound);
    CHECK(w.apply(line(0, Rig::weaver, 0, false)).status == sim::Reject::LineNotFound);
    CHECK(w.apply(line(0, Rig::weaver, 0xFFFFFFFFU, false)).status == sim::Reject::LineNotFound);
    // A class without a switch (the kontor, a plain building) has no line at all.
    CHECK(w.apply(line(0, Rig::kontor, stoffe, false)).status == sim::Reject::LineNotFound);
    CHECK(w.state_hash() == before);
    CHECK(payload<sim::Workshop>(w, Rig::weaver).switched_on);
}

TEST_CASE("unknown ids rejected") {
    Rig r = make_rig();
    sim::World& w = r.world;
    const std::uint64_t before = w.state_hash();
    const sim::GoodsId g = output(r, r.types.weaver);
    // No island holds the tile.
    CHECK(w.apply(line(0, {0, 0}, g, false)).status == sim::Reject::UnknownIsland);
    CHECK(w.apply(line(0, {-5, 105}, g, false)).status == sim::Reject::UnknownIsland);
    CHECK(w.apply(line(0, {164, 110}, g, false)).status == sim::Reject::UnknownIsland);
    CHECK(w.apply(line(0, {110, 164}, g, false)).status == sim::Reject::UnknownIsland);
    CHECK(w.apply(line(0, {0x7FFFFFFF, 0x7FFFFFFF}, g, false)).status ==
          sim::Reject::UnknownIsland);
    CHECK(w.apply(line(0, {-0x7FFFFFFF - 1, -0x7FFFFFFF - 1}, g, false)).status ==
          sim::Reject::UnknownIsland);
    // A neutral tile (the two northern rows of island 0) belongs to no city.
    CHECK(w.apply(line(0, {110, 100}, g, false)).status == sim::Reject::UnknownCity);
    CHECK(w.apply(line(0, {163, 101}, g, false)).status == sim::Reject::UnknownCity);
    // A city's tile without a building.
    CHECK(w.apply(line(0, {130, 130}, g, false)).status == sim::Reject::NoBuildingAtTile);
    CHECK(w.apply(line(0, {110, 111}, g, false)).status == sim::Reject::NoBuildingAtTile);
    // Another player's city: the issuer must own it, both ways round.
    CHECK(w.apply(line(0, Rig::other_weaver, g, false)).status == sim::Reject::NotCityOwner);
    CHECK(w.apply(line(1, Rig::weaver, g, false)).status == sim::Reject::NotCityOwner);
    CHECK(w.apply(line(1, Rig::farm, output(r, r.types.farm), false)).status ==
          sim::Reject::NotCityOwner);
    // The owner of the other city may switch its weaver.
    CHECK(w.state_hash() == before);
    CHECK(w.apply(line(1, Rig::other_weaver, g, false)).ok());
    CHECK(!std::get<sim::Workshop>(building(w, Rig::other_weaver, 3, 1).payload).switched_on);
    CHECK(payload<sim::Workshop>(w, Rig::weaver).switched_on);
}

TEST_CASE("issuer not owner rejected") {
    // The city's owner, not the player who happens to be the host, decides.
    Rig r = make_rig();
    sim::World& w = r.world;
    const sim::GoodsId g = output(r, r.types.weaver);
    // Player 0 is the host and still may not touch player 1's city.
    CHECK(w.apply(line(0, Rig::other_weaver, g, false)).status == sim::Reject::NotCityOwner);
    // Change the owner of city 0 through the test door: now player 1 may, player 0 may not.
    sim::WorldTestAccess::islands(w)[0]->cities[0]->owner = sim::PlayerId{1};
    CHECK(w.apply(line(0, Rig::weaver, g, false)).status == sim::Reject::NotCityOwner);
    CHECK(w.apply(line(1, Rig::weaver, g, false)).ok());
    CHECK(!payload<sim::Workshop>(w, Rig::weaver).switched_on);
}

TEST_CASE("legacy records are refused without a change") {
    // Build, Demolish and the trade commands have their rules since S10
    // (command_build_test.cpp); a legacy record stays closed (§8.3).
    Rig r = make_rig();
    sim::World& w = r.world;
    const std::uint64_t before = w.state_hash();
    const sim::Body bodies[] = {
        sim::LegacyRawCmd{0x6a, {1, 2, 3}},
        sim::LegacyRawCmd{0x87, {}},
        sim::LegacyRawCmd{0x98, {0, 0, 0, 0}},
        sim::LegacyRawCmd{0xffff, {9}},
    };
    for (const sim::Body& b : bodies) {
        const sim::ApplyResult res = w.apply(sim::Command{sim::PlayerId{0}, b});
        CHECK(res.status == sim::Reject::Unsupported);
        CHECK(!res.ok());
        CHECK(res.records.empty());
    }
    CHECK(w.state_hash() == before);
    CHECK(w.find_building(building(w, Rig::weaver).id) != nullptr);
}

TEST_CASE("rejected command leaves the hash unchanged") {
    // A deterministic stream of random commands: every rejected one changes nothing
    // (hash, save bytes and invariants), every accepted one keeps the invariants. The
    // stream has all four reasons to reject and the accepting cases.
    Rig r = make_rig();
    sim::World& w = r.world;
    const sim::GoodsId goods[] = {output(r, r.types.weaver),
                                  output(r, r.types.farm),
                                  output(r, r.types.bow_maker, 0),
                                  output(r, r.types.bow_maker, 1),
                                  output(r, r.types.small_farm, 1),
                                  output(r, r.types.iron_mine),
                                  0,
                                  0x8e,
                                  0xFFFFFFFFU};
    const sim::TileXY tiles[] = {Rig::weaver, Rig::farm,   Rig::bows,         Rig::small_farm,
                                 Rig::mine,   Rig::kontor, Rig::other_weaver, {130, 130},
                                 {110, 100},  {0, 0},      {-1, -1},          {310, 215}};
    const std::uint32_t speeds[] = {0, 50, 100, 200, 400, 75, 0xFFFFFFFFU};
    std::uint32_t x = 20261020;
    auto next = [&x](std::uint32_t n) {
        x = x * 1664525U + 1013904223U;
        return (x >> 12) % n;
    };
    std::size_t accepted = 0, rejected = 0;
    std::vector<std::size_t> by_reason(32, 0);
    for (int i = 0; i < 3000; ++i) {
        const std::uint32_t issuer = next(4); // 0, 1 exist; 2, 3 do not
        sim::Command c;
        if (next(5) == 0) {
            c = speed(issuer, speeds[next(7)]);
        } else if (next(9) == 0) {
            c = sim::Command{sim::PlayerId{issuer}, sim::LegacyRawCmd{0x6a, {}}};
        } else {
            c = line(issuer, tiles[next(12)], goods[next(9)], next(2) == 0);
        }
        const std::uint64_t before = w.state_hash();
        const std::vector<std::uint8_t> bytes_before = w.save();
        const sim::ApplyResult res = w.apply(c);
        if (res.ok()) {
            ++accepted;
        } else {
            ++rejected;
            ++by_reason[static_cast<std::size_t>(res.status)];
            CHECK_MESSAGE(w.state_hash() == before, "reason " << sim::reject_name(res.status));
            CHECK(w.save() == bytes_before);
        }
        if (i % 250 == 0) {
            CHECK(w.check_invariants() == "");
            w.advance(10);
        }
    }
    CHECK(accepted > 100);
    CHECK(rejected > 100);
    for (const sim::Reject reason :
         {sim::Reject::UnknownIssuer, sim::Reject::UnknownIsland, sim::Reject::UnknownCity,
          sim::Reject::NotCityOwner, sim::Reject::NotHost, sim::Reject::BadSpeed,
          sim::Reject::NoBuildingAtTile, sim::Reject::LineNotFound, sim::Reject::Unsupported}) {
        CHECK_MESSAGE(by_reason[static_cast<std::size_t>(reason)] > 0,
                      "no case for " << sim::reject_name(reason));
    }
    CHECK(w.check_invariants() == "");
}

TEST_CASE("a rejection is told to the host and is not state") {
    Rig r = make_rig();
    sim::World& w = r.world;
    w.advance(10);
    static_cast<void>(w.drain_events());
    const std::uint64_t before = w.state_hash();
    CHECK(w.apply(speed(1, 200)).status == sim::Reject::NotHost);
    CHECK(w.apply(speed(7, 200)).status == sim::Reject::UnknownIssuer);
    CHECK(w.apply(speed(0, 200)).ok());
    const std::vector<sim::SimEvent> events = w.drain_events();
    REQUIRE(events.size() == 2); // the accepted command says nothing
    CHECK(events[0].kind == sim::SimEventKind::CommandRejected);
    CHECK(events[0].player == sim::PlayerId{1});
    CHECK(events[0].code == static_cast<std::uint32_t>(sim::Reject::NotHost));
    CHECK(events[0].tick == 5);
    CHECK(events[1].player == sim::PlayerId{7});
    CHECK(events[1].code == static_cast<std::uint32_t>(sim::Reject::UnknownIssuer));
    // The two rejections did not move the hash; only the accepted speed did.
    sim::World twin = r.world;
    twin.apply(speed(0, 100));
    CHECK(twin.state_hash() == before);
}

TEST_CASE("apply is O(command): an accepted command allocates nothing") {
    Rig r = make_rig();
    sim::World& w = r.world;
    const sim::GoodsId g = output(r, r.types.weaver);
    {
        const anno::test::AllocScope probe;
        CHECK(w.apply(speed(0, 200)).ok());
        CHECK(w.apply(line(0, Rig::weaver, g, false)).ok());
        CHECK(w.apply(line(0, Rig::bows, output(r, r.types.bow_maker, 1), false)).ok());
        CHECK(probe.bytes() == 0);
    }
}

namespace {

/// The AI seam of the tests: records what it saw, draws once, and returns the commands
/// it was given on its first call.
struct AiProbe {
    int calls = 0;
    std::vector<std::uint64_t> ticks;
    std::vector<std::uint16_t> stamps_seen;
    std::vector<std::uint16_t> draws;
    std::vector<sim::Command> first_call_commands;
};

void ai_probe(void* ctx, const sim::TickContext& tc, const sim::SerialWorld& sw,
              std::vector<sim::Command>& out) {
    auto& p = *static_cast<AiProbe*>(ctx);
    ++p.calls;
    p.ticks.push_back(tc.tick);
    p.stamps_seen.push_back(sw.players().stamp);
    p.draws.push_back(tc.draw.next(sim::DrawSite::Ai));
    if (p.calls == 1) {
        out = p.first_call_commands;
    }
}

sim::Seams ai_seams(AiProbe& probe) {
    sim::Seams s;
    s.ai.name = "test_ai_probe";
    s.ai.fn = &ai_probe;
    s.ai.ctx = &probe;
    return s;
}

} // namespace

TEST_CASE("the AI seam's commands are applied inside advance, in order") {
    AiProbe probe;
    Rig plain = make_rig();
    probe.first_call_commands = {
        line(0, Rig::weaver, output(plain, plain.types.weaver), false), // applied
        line(1, Rig::weaver, output(plain, plain.types.weaver), true),  // not the owner
        speed(0, 200),                                                  // applied, host
        speed(0, 123),                                                  // bad speed
        line(0, Rig::farm, output(plain, plain.types.farm), false),     // applied
    };
    Rig r = make_rig(ai_seams(probe));
    sim::World& w = r.world;
    REQUIRE(w.check_invariants() == "");
    REQUIRE(payload<sim::Workshop>(w, Rig::weaver).switched_on);

    w.advance(10);
    CHECK(probe.calls == 1);
    CHECK(probe.ticks == std::vector<std::uint64_t>{5});
    // Applied at once, in order: the second line command (rejected) did not undo the
    // first; the speed arrived; the farm is off.
    CHECK(!payload<sim::Workshop>(w, Rig::weaver).switched_on);
    CHECK(w.settings().speed_percent == 200);
    CHECK(!payload<sim::Farm>(w, Rig::farm).switched_on);
    // The two rejections are told in the advance's output and to the host queue.
    int rejected = 0;
    std::vector<std::uint32_t> codes;
    for (const sim::SimEvent& e : w.last_output().events) {
        if (e.kind == sim::SimEventKind::CommandRejected) {
            ++rejected;
            codes.push_back(e.code);
        }
    }
    CHECK(rejected == 2);
    CHECK(codes == std::vector<std::uint32_t>{static_cast<std::uint32_t>(sim::Reject::NotCityOwner),
                                              static_cast<std::uint32_t>(sim::Reject::BadSpeed)});
    int host_rejected = 0;
    for (const sim::SimEvent& e : w.drain_events()) {
        host_rejected += e.kind == sim::SimEventKind::CommandRejected ? 1 : 0;
    }
    CHECK(host_rejected == 2);

    // Every advance calls the AI once, whatever the step, and its draw is told.
    for (int i = 0; i < 9; ++i) {
        w.advance(static_cast<std::uint32_t>(1 + i));
    }
    CHECK(probe.calls == 10);
    CHECK(probe.draws.size() == 10);
    bool saw_ai_draw = false;
    for (const sim::DrawRecord& d : w.last_output().draws) {
        if (d.site == sim::DrawSite::Ai) {
            saw_ai_draw = true;
            CHECK(d.count == 1);
        }
    }
    CHECK(saw_ai_draw);
    // The AI runs after the players were visited and before the table's stamp is written
    // (D 0x1009c4d0): at the first call the stamp is still the old one (0), at the next it
    // is the previous tick's.
    CHECK(probe.stamps_seen[0] == 0);
    CHECK(probe.stamps_seen[1] == static_cast<std::uint16_t>(probe.ticks[0]));
    CHECK(w.check_invariants() == "");
}

TEST_CASE("the AI seam is part of the replay identity and its commands equal host commands") {
    // The same commands, once from the AI seam and once from the host before the advance,
    // reach the same state except for what the seam itself costs (its draw moves the
    // cursor, its name is in the header).
    AiProbe probe;
    Rig base = make_rig();
    const sim::GoodsId g = output(base, base.types.weaver);
    probe.first_call_commands = {line(0, Rig::weaver, g, false)};
    Rig with_ai = make_rig(ai_seams(probe));
    sim::World host = base.world;
    CHECK(host.apply(line(0, Rig::weaver, g, false)).ok());
    host.advance(10);
    with_ai.world.advance(10);
    CHECK(payload<sim::Workshop>(host, Rig::weaver).switched_on ==
          payload<sim::Workshop>(with_ai.world, Rig::weaver).switched_on);
    CHECK(host.state_hash() != with_ai.world.state_hash()); // seam name, one draw
    // The seam's name is hashed: without the seam the world hashes differently from the
    // start.
    CHECK(base.world.state_hash() != with_ai.world.state_hash());
    // An AI that returns nothing and draws nothing changes nothing but the header.
    sim::Seams quiet;
    quiet.ai.name = "quiet";
    quiet.ai.fn = [](void*, const sim::TickContext&, const sim::SerialWorld&,
                     std::vector<sim::Command>&) {};
    Rig q = make_rig(quiet);
    sim::World plain = base.world;
    q.world.advance(10);
    plain.advance(10);
    CHECK(q.world.last_output().events.empty());
    CHECK(payload<sim::Workshop>(q.world, Rig::weaver).out[0] ==
          payload<sim::Workshop>(plain, Rig::weaver).out[0]);
    CHECK(q.world.rng().cursor() == plain.rng().cursor());
}
