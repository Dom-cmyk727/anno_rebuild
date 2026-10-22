// Stage S10 property tests (docs/sim/state.md §14.4, §15 S10): random command streams
// keep the invariants and a rejected command changes nothing; the golden G2 (§14.3):
// the G1 scenario driven by a recorded command log, committed as
// fixtures/synthetic/sim/g2.log and replayed checkpoint by checkpoint.
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/world.hpp>

#include "command_helpers.hpp"
#include "golden.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;
namespace ct = command_test;

namespace {

using ct::at;
using ct::kCity0;
using ct::kCity1;

/// A deterministic LCG (the test's own; seeds logged on failure).
struct Lcg {
    std::uint32_t x;
    std::uint32_t next(std::uint32_t n) {
        x = x * 1664525U + 1013904223U;
        return (x >> 8) % n;
    }
};

/// Puts the money and the build goods of both players back, so that the stream keeps
/// placing buildings instead of running into CannotAfford only.
void top_up(sim::World& w, const syn::SyntheticTypes& t) {
    for (const std::uint32_t p : {0U, 1U}) {
        sim::WorldTestAccess::players(w).slots[p]->money = 10000;
    }
    for (const sim::CityRef c : {kCity0, kCity1}) {
        if (sim::WorldTestAccess::islands(w)[0]->city(c.city) != nullptr) {
            sim::City& city = ct::city_mut(w, c);
            city.stock.stock[ct::goods(t, "WERKZEUG")] = 13 * sim::kTon;
            city.stock.stock[ct::goods(t, "HOLZ")] = 30 * sim::kTon;
        }
    }
}

/// One random command. Half of the tiles are drawn from `hot` (the tiles where buildings
/// were placed), so that Demolish and SetProductionLine meet buildings often.
sim::Command random_command(Lcg& g, const syn::SyntheticTypes& t,
                            const std::vector<sim::TileXY>& hot) {
    const std::array<sim::TypeId, 13> types = {t.weaver,     t.farm,         t.fur_trapper,
                                               t.small_farm, t.iron_mine,    t.pioneer_house,
                                               t.kontor,     t.market_stall, t.bow_maker,
                                               t.road,       sim::TypeId{0}, sim::TypeId{4242},
                                               sim::TypeId{}};
    const std::array<sim::CityRef, 5> cities = {kCity0, kCity1, kCity0,
                                                sim::CityRef{sim::IslandId{0}, sim::CityNo{7}},
                                                sim::CityRef{sim::IslandId{9}, sim::CityNo{0}}};
    const auto issuer = sim::PlayerId{g.next(3)}; // 0 and 1 exist, 2 does not
    const sim::CityRef city = cities[g.next(5)];
    const auto tile = [&g, &hot]() {
        if (!hot.empty() && g.next(2) == 0) {
            return hot[g.next(static_cast<std::uint32_t>(hot.size()))];
        }
        const auto x = static_cast<std::int32_t>(96 + g.next(72));
        const auto y = static_cast<std::int32_t>(96 + g.next(72));
        return sim::TileXY{x, y};
    };
    switch (g.next(10)) {
    case 0:
    case 1:
    case 2:
    case 3: {
        sim::BuildCmd b{city, {}};
        const std::uint32_t n = 1 + g.next(4);
        for (std::uint32_t i = 0; i < n; ++i) {
            // One draw per statement: the order of function arguments is unspecified.
            const sim::TileXY p = tile();
            const sim::TypeId type = types[g.next(13)];
            const auto rotation = static_cast<std::uint8_t>(g.next(5));
            b.placements.push_back(at(type, p.x, p.y, rotation));
        }
        return sim::Command{issuer, b};
    }
    case 4:
    case 5: {
        sim::DemolishCmd d{city, {}};
        const std::uint32_t n = 1 + g.next(3);
        for (std::uint32_t i = 0; i < n; ++i) {
            d.tiles.push_back(tile());
        }
        return sim::Command{issuer, d};
    }
    case 6: {
        // Braced initialisers are evaluated left to right.
        return sim::Command{issuer,
                            sim::SetProductionLineCmd{tile(), g.next(0x40), g.next(2) == 0}};
    }
    case 7: {
        const std::array<std::uint16_t, 4> modes = {0, 1, 9, 3};
        const std::array<std::uint32_t, 5> values = {0xffffffffU, 0, 1, 2, 77};
        const std::uint16_t mode = modes[g.next(4)];
        const sim::GoodsId goods = g.next(0x3a);
        const std::uint32_t value = values[g.next(5)];
        return ct::trade_flag(issuer.v, city, mode, goods, value);
    }
    case 8: {
        const std::array<std::uint32_t, 4> indexes = {sim::kAppendIndex, 0, 1, 5};
        const sim::GoodsId goods = g.next(3) == 0 ? 0 : g.next(0x3a);
        const std::uint32_t list = g.next(3);
        const std::uint32_t index = indexes[g.next(4)];
        const std::uint32_t amount = g.next(1000);
        const std::uint32_t price = g.next(1000);
        return ct::trade_slot(issuer.v, city, list, index, goods, amount, price);
    }
    default:
        if (g.next(2) == 0) {
            return sim::Command{issuer, sim::SetSpeedCmd{g.next(2) == 0 ? 200U : 123U}};
        }
        return sim::Command{issuer, sim::LegacyRawCmd{0x6a, {1, 2}}};
    }
}

} // namespace

TEST_CASE("random command streams keep invariants") {
    // §14.4: random Build / Demolish / SetLine / SetSpeed / trade / legacy commands from
    // existing and missing players for existing and missing cities, with advances in
    // between; after every command the invariants hold, and a rejected command leaves the
    // hash (and, sampled, the save) unchanged. Run once per value of Rules::build_atomic.
    const syn::SyntheticTypes t = ct::build_types();
    for (const bool atomic : {false, true}) {
        CAPTURE(atomic);
        sim::Rules rules;
        rules.build_atomic = atomic;
        sim::World w = ct::make_world(t, ct::two_city_spec(t), rules);
        Lcg g{atomic ? 20261024U : 20261023U};
        std::vector<sim::TileXY> hot = {{110, 110}, {120, 110}, {110, 140}};
        std::map<sim::Reject, std::size_t> seen; // command and record statuses
        std::size_t accepted = 0, placed = 0, removed = 0;
        for (int i = 0; i < 1500; ++i) {
            CAPTURE(i);
            if (i % 100 == 0) {
                top_up(w, t);
            }
            const sim::Command c = random_command(g, t, hot);
            const std::uint64_t before = w.state_hash();
            const bool sample = i % 50 == 0;
            const std::vector<std::uint8_t> bytes = sample ? w.save() : std::vector<std::uint8_t>{};
            const sim::ApplyResult res = w.apply(c);
            ++seen[res.status];
            for (const sim::RecordResult& r : res.records) {
                ++seen[r.status];
                if (r.status == sim::Reject::None && r.building.valid()) {
                    if (std::holds_alternative<sim::BuildCmd>(c.body)) {
                        ++placed;
                    } else {
                        ++removed;
                    }
                }
            }
            if (const auto* b = std::get_if<sim::BuildCmd>(&c.body)) {
                for (std::size_t k = 0; k < res.records.size(); ++k) {
                    if (res.records[k].building.valid()) {
                        hot.push_back(sim::TileXY{b->placements[k].x, b->placements[k].y});
                    }
                }
            }
            if (res.ok()) {
                ++accepted;
            } else {
                CHECK_MESSAGE(w.state_hash() == before, "reason " << sim::reject_name(res.status));
                if (sample) {
                    CHECK(w.save() == bytes);
                }
            }
            const std::string problem = w.check_invariants();
            REQUIRE_MESSAGE(problem.empty(), problem);
            if (i % 25 == 24) {
                w.advance(1 + g.next(100));
                const std::string after = w.check_invariants();
                REQUIRE_MESSAGE(after.empty(), after);
            }
        }
        CHECK(accepted > 100);
        // Under the atomic rule a drag build with one bad record places nothing, so
        // fewer buildings go up.
        CHECK(placed > (atomic ? 5U : 20U));
        CHECK(removed > 5);
        for (const sim::Reject reason :
             {sim::Reject::UnknownIssuer, sim::Reject::UnknownIsland, sim::Reject::UnknownCity,
              sim::Reject::NotCityOwner, sim::Reject::UnknownType, sim::Reject::BadRotation,
              sim::Reject::Unsupported, sim::Reject::OutOfIsland, sim::Reject::TileNotOwned,
              sim::Reject::Occupied, sim::Reject::CannotAfford, sim::Reject::NoBuildingAtTile,
              sim::Reject::LineNotFound, sim::Reject::BadIndex, sim::Reject::BadValue}) {
            CHECK_MESSAGE(seen[reason] > 0, "no case for " << sim::reject_name(reason));
        }
        if (atomic) {
            CHECK(seen[sim::Reject::AtomicAbort] > 0);
        }
        // The stream is a pure function of the seed: the same commands on a fresh world
        // give the same hash (determinism, §14.4).
        sim::World again = ct::make_world(t, ct::two_city_spec(t), rules);
        Lcg h{atomic ? 20261024U : 20261023U};
        std::vector<sim::TileXY> hot2 = {{110, 110}, {120, 110}, {110, 140}};
        for (int i = 0; i < 1500; ++i) {
            if (i % 100 == 0) {
                top_up(again, t);
            }
            const sim::Command c = random_command(h, t, hot2);
            const sim::ApplyResult res = again.apply(c);
            if (const auto* b = std::get_if<sim::BuildCmd>(&c.body)) {
                for (std::size_t k = 0; k < res.records.size(); ++k) {
                    if (res.records[k].building.valid()) {
                        hot2.push_back(sim::TileXY{b->placements[k].x, b->placements[k].y});
                    }
                }
            }
            if (i % 25 == 24) {
                again.advance(1 + h.next(100));
            }
        }
        CHECK(again.state_hash() == w.state_hash());
    }
}

namespace {

std::filesystem::path g2_path() {
    return std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "g2.log";
}

/// What the G2 session checks besides the hashes.
struct G2Session {
    sim::ReplayLog log;
    std::vector<sim::ApplyResult> results;
};

void advance_n(sim::Recorder& rec, int n, std::uint32_t ms) {
    for (int i = 0; i < n; ++i) {
        rec.advance(ms);
    }
}

/// The G2 session (docs/sim/state.md §14.3): the G1 scenario on the types with build
/// costs, single-player steps of 100 ms (50 ticks), a second weaver at tick 5 000, its
/// line off and on, a third and a fourth weaver around the demolition of the farm (the
/// free-slot order of §15 S10), a drag build that runs out of tools, trade settings, and
/// rejected commands in between. A checkpoint before the first command and after each
/// block.
G2Session record_g2(sim::World& w, const syn::SyntheticTypes& t) {
    auto started = sim::Recorder::start(w);
    REQUIRE_MESSAGE(started.ok(), (started.ok() ? std::string() : started.error()));
    sim::Recorder rec = std::move(started).value();
    G2Session s;
    const sim::GoodsId stoffe = t.registry->find(t.weaver)->output_goods.at(0);
    const sim::GoodsId wool = ct::goods(t, "WOLLE");
    const sim::GoodsId wood = ct::goods(t, "HOLZ");
    auto submit = [&](const sim::Command& c) { s.results.push_back(rec.submit(c)); };
    rec.checkpoint();
    advance_n(rec, 100, 100); // tick 5 000
    rec.checkpoint();
    submit(ct::build(0, kCity0, {at(t.weaver, 112, 110)})); // [0] slot 2
    submit(ct::build(5, kCity0, {at(t.weaver, 113, 110)})); // [1] UnknownIssuer
    submit(ct::build(0, kCity0, {at(t.weaver, 108, 103)})); // [2] Occupied
    submit(ct::build(0, kCity0, {at(t.weaver, 110, 100)})); // [3] TileNotOwned
    advance_n(rec, 50, 100);                                // tick 7 500
    rec.checkpoint();
    submit(sim::Command{sim::PlayerId{0}, sim::SetProductionLineCmd{{112, 110}, stoffe, false}});
    submit(sim::Command{sim::PlayerId{0}, sim::SetProductionLineCmd{{120, 110}, stoffe, false}});
    advance_n(rec, 50, 100); // tick 10 000
    rec.checkpoint();
    submit(sim::Command{sim::PlayerId{0}, sim::SetProductionLineCmd{{112, 110}, stoffe, true}});
    submit(ct::build(0, kCity0, {at(t.weaver, 114, 110)})); // [7] slot 3
    submit(ct::demolish(0, kCity0, {{120, 110}}));          // [8] the farm, slot 1
    submit(ct::build(0, kCity0, {at(t.weaver, 116, 110)})); // [9] slot 4
    submit(ct::demolish(0, kCity0, {{0, 0}}));              // [10] skipped tile
    submit(ct::trade_slot(0, kCity0, 0, sim::kAppendIndex, wool, 4 * sim::kTon, 30));
    submit(ct::trade_flag(0, kCity0, 0, wool, 0));
    submit(ct::trade_flag(0, kCity0, 9, wood, 5));
    submit(sim::Command{sim::PlayerId{0}, sim::LegacyRawCmd{0x6a, {1, 2, 3}}}); // [14]
    advance_n(rec, 100, 100);                                                   // tick 15 000
    rec.checkpoint();
    submit(ct::build(0, kCity0,
                     {at(t.weaver, 118, 110), at(t.weaver, 110, 112), at(sim::TypeId{0}, 0, 0),
                      at(sim::TypeId{4242}, 111, 112)})); // [15] slot 5, then out of tools
    advance_n(rec, 100, 100);                             // tick 20 000
    rec.checkpoint();
    s.log = rec.log();
    return s;
}

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in),
                                     std::istreambuf_iterator<char>());
}

} // namespace

TEST_CASE("G2 replay equals golden") {
    const syn::SyntheticTypes t = ct::build_types();
    sim::World w = ct::make_world(t, syn::g2_scenario(t));
    const G2Session s = record_g2(w, t);

    // Semantic asserts (independent of the hash).
    REQUIRE(s.results.size() == 16);
    CHECK(s.results[0].ok());
    CHECK(s.results[1].status == sim::Reject::UnknownIssuer);
    CHECK(s.results[2].status == sim::Reject::Occupied);
    CHECK(s.results[3].status == sim::Reject::TileNotOwned);
    CHECK(s.results[4].ok());
    CHECK(s.results[5].status == sim::Reject::LineNotFound);
    CHECK(s.results[14].status == sim::Reject::Unsupported);
    REQUIRE(s.results[15].records.size() == 4);
    CHECK(s.results[15].records[0].status == sim::Reject::None);
    CHECK(s.results[15].records[1].status == sim::Reject::CannotAfford);
    CHECK(s.results[15].records[2].status == sim::Reject::None); // type 0: skipped
    CHECK(s.results[15].records[3].status == sim::Reject::UnknownType);
    // The free-slot order of §15 S10: slots 2, 3, (1 freed, hint kept), 4, 5.
    const sim::BuildingList& list = ct::production_list(w, kCity0, t);
    const auto slot_of = [&](std::size_t result) {
        return ct::key_of(w, s.results[result].records.at(0).building).slot;
    };
    CHECK(slot_of(0) == 2);
    CHECK(slot_of(7) == 3);
    CHECK(slot_of(9) == 4);
    CHECK(slot_of(15) == 5);
    CHECK_FALSE(list.slots[1].has_value());
    CHECK(list.slots.size() == 10);
    CHECK(list.hint == 5);
    // Four weavers paid: 1 200 gold, 12 t of 13 t tools, 16 t of 30 t wood.
    CHECK(w.players().find(sim::PlayerId{0})->money == 10000 - 4 * 300);
    CHECK(ct::city(w, kCity0).stock.stock.at(ct::goods(t, "WERKZEUG")) == 1 * sim::kTon);
    CHECK(ct::city(w, kCity0).stock.stock.at(ct::goods(t, "HOLZ")) == 14 * sim::kTon);
    CHECK(w.clock().tick() == 20000);
    CHECK(w.check_invariants() == "");

    // The log is byte-identical to the committed one (ANNO_UPDATE_GOLDEN=1 rewrites it).
    auto written = sim::write_log(s.log);
    REQUIRE_MESSAGE(written.ok(), (written.ok() ? std::string() : written.error()));
    if (anno::test::update_mode()) {
        std::ofstream out(g2_path(), std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(written.value().data()),
                  static_cast<std::streamsize>(written.value().size()));
        MESSAGE("wrote " << g2_path().string());
    }
    const std::vector<std::uint8_t> committed = read_bytes(g2_path());
    CHECK_MESSAGE(committed == written.value(), "the recorded G2 log differs from "
                                                    << g2_path().string()
                                                    << " (ANNO_UPDATE_GOLDEN=1 rewrites it)");

    // The committed log replays: every checkpoint matches on a world rebuilt from its
    // initial save.
    auto log = sim::read_log(committed);
    REQUIRE_MESSAGE(log.ok(), (log.ok() ? std::string() : log.error()));
    auto fresh = sim::World::load(log.value().initial_save, t.registry);
    REQUIRE_MESSAGE(fresh.ok(), (fresh.ok() ? std::string() : fresh.error()));
    sim::World replayed = std::move(fresh).value();
    auto outcome = sim::replay(replayed, log.value());
    REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
    CHECK(outcome.value().reproduced());
    CHECK(outcome.value().checkpoints_total == 6);
    CHECK(outcome.value().checkpoints_matched == 6);
    CHECK(replayed.state_hash() == w.state_hash());

    // The final hash is a golden of its own (goldens.txt).
    std::string line;
    const std::string problem = anno::test::check_golden("s10_g2_end", "S10", w.state_hash(), line);
    CHECK_MESSAGE(problem.empty(), problem);
    if (anno::test::update_mode()) {
        MESSAGE("golden line: " << line);
    }
}

TEST_CASE("run_turn without a recorder skips a command without a wire form") {
    // The S9 gap closed in S10 (docs/sim/state.md §15 S9 fix-up note): a Build of more
    // placements than one record holds (4 095) has no wire form, so the recorder refuses
    // it; the unrecorded session must not apply it either, or the two sessions of the same
    // commands would differ.
    const syn::SyntheticTypes t = ct::build_types();
    sim::BuildCmd huge{kCity0, {at(t.weaver, 112, 115)}};
    huge.placements.resize(4096, at(sim::TypeId{0}, 0, 0));
    REQUIRE_FALSE(sim::has_wire_form(sim::Body{huge}));
    sim::BuildCmd fits = huge;
    fits.placements.resize(4095);
    REQUIRE(sim::has_wire_form(sim::Body{fits}));
    REQUIRE(sim::encode_record(sim::Body{fits}).ok());
    REQUIRE_FALSE(sim::encode_record(sim::Body{huge}).ok());

    sim::World direct = ct::make_world(t, syn::g2_scenario(t));
    sim::World recorded = direct;
    auto started = sim::Recorder::start(recorded);
    REQUIRE(started.ok());
    sim::Recorder rec = std::move(started).value();
    sim::Turn turn;
    turn.step_ms = 10;
    turn.commands = {sim::Command{sim::PlayerId{0}, huge}};
    sim::run_turn(direct, nullptr, turn, 2);
    sim::run_turn(recorded, &rec, turn, 2);
    CHECK(direct.building_at(kCity0, 112, 115) == nullptr);
    CHECK(direct.state_hash() == recorded.state_hash());
    CHECK(rec.log().entries.size() == 2); // the two advances, no command
    // The longest Build that fits is applied on both paths.
    turn.commands = {sim::Command{sim::PlayerId{0}, fits}};
    sim::run_turn(direct, nullptr, turn, 1);
    sim::run_frame(recorded, &rec, turn.commands, 10);
    CHECK(direct.building_at(kCity0, 112, 115) != nullptr);
    CHECK(direct.state_hash() == recorded.state_hash());
}
