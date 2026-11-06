// R2 public authority/atomicity, independent wire/native schema and real old refusal.
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

#include <anno/formats/json.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/state_fields.hpp>

#include "freight_helpers.hpp"
#include "island_helpers.hpp"
namespace sim = anno::sim;
namespace ft = freight_test;
namespace json = anno::formats::json;
namespace {
const json::Value& at(const json::Value& v, const char* name) {
    const auto* found = v.find(name);
    REQUIRE(found != nullptr);
    return *found;
}
std::uint64_t number(const json::Value& v) {
    REQUIRE(v.kind == json::Kind::Integer);
    REQUIRE(v.integer >= 0);
    return static_cast<std::uint64_t>(v.integer);
}
std::vector<std::uint8_t> unhex(const std::string& s) {
    REQUIRE(s.size() % 2 == 0);
    std::vector<std::uint8_t> out;
    for (std::size_t i = 0; i < s.size(); i += 2)
        out.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i, 2), nullptr, 16)));
    return out;
}
std::vector<sim::FreightStop> stops(const json::Value& v) {
    std::vector<sim::FreightStop> out;
    for (const auto& s : v.items) {
        sim::FreightStop stop;
        stop.warehouse = sim::BuildingId{number(at(s, "warehouse"))};
        stop.dock_x_q8 = static_cast<std::int32_t>(number(at(s, "dock_x")));
        stop.dock_y_q8 = static_cast<std::int32_t>(number(at(s, "dock_y")));
        for (const auto& a : at(s, "actions").items)
            stop.actions.push_back({static_cast<sim::FreightDirection>(number(at(a, "direction"))),
                                    static_cast<sim::GoodsId>(number(at(a, "goods"))),
                                    number(at(a, "tons"))});
        out.push_back(stop);
    }
    return out;
}
std::vector<std::uint8_t> bytes(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    REQUIRE(in.good());
    return {std::istreambuf_iterator<char>(in), {}};
}
} // namespace
TEST_CASE("freight schema: nonempty wire and native bytes equal independent hand answers") {
    std::ifstream in(std::filesystem::path(ANNO_FIXTURES_DIR) /
                     "sim/freight/contract_vectors.jsonl");
    REQUIRE(in.good());
    unsigned compared = 0;
    for (std::string line; std::getline(in, line);) {
        auto parsed =
            json::parse({reinterpret_cast<const std::uint8_t*>(line.data()), line.size()});
        REQUIRE(parsed);
        const auto& row = parsed.value();
        const auto& kind = at(row, "kind").text;
        if (kind != "wire" && kind != "assign_wire" && kind != "native")
            continue;
        INFO(at(row, "name").text);
        ++compared;
        const auto& input = at(row, "input");
        const auto want = unhex(at(at(row, "expect"), "hex").text);
        if (kind == "wire" || kind == "assign_wire") {
            sim::Body body =
                sim::ControlFreightRouteCmd{sim::FigureId{number(at(input, "ship"))},
                                            static_cast<sim::FreightControl>(
                                                kind == "wire" ? number(at(input, "control")) : 0)};
            if (kind == "assign_wire")
                body = sim::AssignFreightRouteCmd{sim::FigureId{number(at(input, "ship"))},
                                                  stops(at(input, "stops"))};
            const auto encoded = sim::encode_record(body);
            REQUIRE(encoded);
            CHECK(encoded.value() == want);
            const auto decoded = sim::decode_record(want);
            REQUIRE(decoded);
            CHECK(decoded.value() == body);
            for (std::size_t cut = 0; cut < want.size(); ++cut)
                CHECK_FALSE(sim::decode_record(std::span(want).first(cut)));
            auto extra = want;
            extra.push_back(0);
            CHECK_FALSE(sim::decode_record(extra));
            if (kind == "assign_wire") {
                auto wrong = want;
                wrong[36] = 2;
                CHECK_FALSE(sim::decode_record(wrong));
                wrong = want;
                for (unsigned i = 12; i < 16; ++i)
                    wrong[i] = 255;
                CHECK_FALSE(sim::decode_record(wrong));
            } else {
                auto wrong = want;
                wrong.back() = 3;
                CHECK_FALSE(sim::decode_record(wrong));
            }
        } else {
            sim::Figure f;
            f.freight_generation = number(at(input, "generation"));
            const auto& state = at(input, "state");
            if (state.kind != json::Kind::Null) {
                auto& r = f.freight.emplace();
                r.stops = stops(at(state, "stops"));
                r.status = static_cast<sim::FreightStatus>(number(at(state, "status")));
                r.phase = static_cast<sim::FreightPhase>(number(at(state, "phase")));
                r.stop_index = static_cast<std::uint32_t>(number(at(state, "stop_index")));
                r.action_index = static_cast<std::uint32_t>(number(at(state, "action_index")));
                r.expected_order_id = number(at(state, "expected_order_id"));
                r.order_generation = number(at(state, "order_generation"));
                r.last_tick = number(at(state, "last_tick"));
                r.credit_ticks = number(at(state, "credit_ticks"));
                r.wait_remaining = static_cast<std::uint32_t>(number(at(state, "wait_remaining")));
                r.scan_next = static_cast<std::uint32_t>(number(at(state, "scan_next")));
                r.matching_raw = number(at(state, "matching_raw"));
                r.remaining_raw = number(at(state, "remaining_raw"));
                for (const auto& i : at(state, "matching_holds").items)
                    r.matching_holds.push_back(static_cast<std::uint32_t>(number(i)));
                for (const auto& i : at(state, "empty_holds").items)
                    r.empty_holds.push_back(static_cast<std::uint32_t>(number(i)));
                r.candidate_next = static_cast<std::uint32_t>(number(at(state, "candidate_next")));
                r.completed_cycles = number(at(state, "completed_cycles"));
                r.last_reject = static_cast<sim::Reject>(number(at(state, "last_reject")));
                r.nav_failure = static_cast<std::uint8_t>(number(at(state, "nav_failure")));
            }
            sim::WriteArchive a;
            a.u64(f.freight_generation);
            sim::ar::opt(a, f.freight, [&](const auto& r) { sim::freight_fields(a, r, 3); });
            CHECK(a.data() == want);
            sim::WriteArchive full;
            sim::fields(full, f);
            REQUIRE(full.data().size() >= want.size());
            CHECK(std::equal(want.rbegin(), want.rend(), full.data().rbegin()));
            sim::Figure read;
            sim::ReadArchive reader(full.data());
            sim::fields(reader, read);
            // The independent suffix has candidates in a three-hold domain.
            // Full-figure input must actually contain those holds before reading it.
            if (f.freight.has_value() && !f.freight->matching_holds.empty()) {
                f.holds.resize(3);
                sim::WriteArchive valid;
                sim::fields(valid, f);
                sim::ReadArchive rr(valid.data());
                sim::fields(rr, read);
                CHECK_FALSE(rr.failed());
            } else
                CHECK_FALSE(reader.failed());
            CHECK(read.freight_generation == f.freight_generation);
            CHECK(read.freight.has_value() == f.freight.has_value());
            if (f.freight.has_value())
                CHECK(*read.freight == *f.freight);
        }
    }
    CHECK(compared == 6);
    sim::WriteArchive rules;
    const sim::FreightRules rule_value;
    sim::fields(rules, rule_value);
    CHECK(rules.data() == unhex("000400000008000040000000"));
    CHECK(sim::Body{sim::AssignFreightRouteCmd{}}.index() == 12);
    CHECK(sim::Body{sim::ControlFreightRouteCmd{}}.index() == 13);
    CHECK_FALSE(sim::has_wire_form(sim::LegacyRawCmd{sim::kWireAssignFreight, {}}));
    CHECK_FALSE(sim::has_wire_form(sim::AssignFreightRouteCmd{}));
}
TEST_CASE("freight commands: live authority canonical grouping deep copies and atomic refusals") {
    auto w = ft::make(1, true);
    auto route = ft::route(w, 91);
    route.stops.push_back(route.stops.front());
    route.stops[0].actions = {{sim::FreightDirection::LoadTarget, 91, 2},
                              {sim::FreightDirection::UnloadAmount, 91, 1},
                              {sim::FreightDirection::LoadTarget, 91, 3}};
    REQUIRE(ft::apply(w, route).ok());
    const auto* r = w.freight_route(ft::ship_id);
    REQUIRE(r != nullptr);
    CHECK(r->stops.size() == 3);
    CHECK(r->phase == sim::FreightPhase::NeedLeg);
    CHECK(r->stops[0].actions[0].tons == 5);
    CHECK(r->stops[0].actions[2].tons == 0);
    CHECK_FALSE(ft::ship(w).nav->order);
    CHECK(ft::ship(w).freight_generation == 1);
    CHECK(w.check_invariants().empty());
    const auto saved = w.save();
    auto loaded = sim::World::load(saved, ft::registry(true));
    REQUIRE(loaded);
    CHECK(loaded.value().save() == saved);
    CHECK(loaded.value().state_hash() == w.state_hash());
    auto copy = w;
    ft::ship(copy).freight->stops[0].actions[0].tons = 6;
    CHECK(w.freight_route(ft::ship_id)->stops[0].actions[0].tons == 5);
    const auto refuse = [&](sim::Body body, sim::Reject why, unsigned owner = 0) {
        const auto before = w.save();
        const auto rng = w.rng().cursor();
        CHECK(w.apply({sim::PlayerId{owner}, std::move(body)}).status == why);
        CHECK(w.save() == before);
        CHECK(w.rng().cursor() == rng);
    };
    refuse(route, sim::Reject::NotShipOwner, 1);
    refuse(route, sim::Reject::UnknownIssuer, 9);
    auto bad = route;
    bad.ship = sim::FigureId{99};
    refuse(bad, sim::Reject::UnknownFigure);
    bad = route;
    bad.stops.clear();
    refuse(bad, sim::Reject::BadValue);
    bad = route;
    bad.stops[0].dock_x_q8 = -1;
    refuse(bad, sim::Reject::BadValue);
    bad = route;
    bad.stops[0].warehouse = ft::warehouse(w, 2);
    refuse(bad, sim::Reject::NotCityOwner);
    bad = route;
    bad.stops[0].warehouse = sim::BuildingId{999};
    refuse(bad, sim::Reject::NoBuildingAtTile);
    bad = route;
    bad.stops[0].actions[0].goods = 101;
    refuse(bad, sim::Reject::UnsupportedGoods);
    bad = route;
    bad.stops[0].actions[0].tons = UINT64_MAX;
    refuse(bad, sim::Reject::BadValue);
    bad = route;
    bad.stops[0].actions[0].tons = UINT64_MAX / 256;
    bad.stops[0].actions[2].tons = 1;
    refuse(bad, sim::Reject::BadValue);
    bad = route;
    bad.stops.resize(w.rules().freight.max_stops + 1);
    refuse(bad, sim::Reject::BadValue);
    refuse(sim::ControlFreightRouteCmd{ft::ship_id, static_cast<sim::FreightControl>(9)},
           sim::Reject::BadValue);
    refuse(sim::MoveCmd{ft::ship_id, -1, 0}, sim::Reject::BadValue);
    REQUIRE(w.freight_route(ft::ship_id) != nullptr);
    REQUIRE(
        ft::apply(w, sim::ControlFreightRouteCmd{ft::ship_id, sim::FreightControl::Suspend}).ok());
    const auto suspended = w.save();
    REQUIRE(
        ft::apply(w, sim::ControlFreightRouteCmd{ft::ship_id, sim::FreightControl::Suspend}).ok());
    CHECK(w.save() == suspended);
    ft::ship(w).state_word = 2;
    refuse(sim::ControlFreightRouteCmd{ft::ship_id, sim::FreightControl::Resume},
           sim::Reject::ShipDisabled);
    REQUIRE(
        ft::apply(w, sim::ControlFreightRouteCmd{ft::ship_id, sim::FreightControl::Cancel}).ok());
    CHECK(w.freight_route(ft::ship_id) == nullptr);
    CHECK(ft::ship(w).freight_generation == 1);
    ft::ship(w).state_word = 0;
    REQUIRE(ft::apply(w, sim::MoveCmd{ft::ship_id, 90 * 256, 110 * 256}).ok());
    const auto manual = w.save();
    REQUIRE(
        ft::apply(w, sim::ControlFreightRouteCmd{ft::ship_id, sim::FreightControl::Cancel}).ok());
    CHECK(w.save() == manual);
    refuse(sim::ControlFreightRouteCmd{ft::ship_id, sim::FreightControl::Resume},
           sim::Reject::BadValue);
    ft::ship(w).freight_generation = UINT64_MAX;
    refuse(route, sim::Reject::SerialExhausted);
}
TEST_CASE("freight native: corrupt continuation refuses and real134 cannot enter136") {
    auto w = ft::make();
    REQUIRE(ft::apply(w, ft::route(w)).ok());
    const auto check = [&](auto edit) {
        auto bad = w;
        edit(*ft::ship(bad).freight);
        const auto result = sim::World::load(bad.save(), ft::registry());
        REQUIRE_FALSE(result);
        INFO(result.error());
        if (static_cast<unsigned>(ft::ship(bad).freight->status) > 2)
            CHECK(result.error().find("enum value out of range") != std::string::npos);
        else
            CHECK(result.error().find("freight") != std::string::npos);
    };
    check([](auto& r) { r.stop_index = 2; });
    check([](auto& r) { r.last_tick = 1; });
    check([](auto& r) { r.credit_ticks = 1; });
    check([](auto& r) { r.order_generation = 1; });
    check([](auto& r) { r.scan_next = 1; });
    check([](auto& r) { r.stops[0].warehouse = sim::BuildingId{999}; });
    check([](auto& r) { r.matching_holds = {0, 0}; });
    check([](auto& r) { r.stops[0].actions[0].tons = UINT64_MAX; });
    check([](auto& r) { r.status = static_cast<sim::FreightStatus>(3); });
    check([](auto& r) { r.stops[0].actions.push_back(r.stops[0].actions[0]); });
    const auto base = std::filesystem::path(ANNO_FIXTURES_DIR) / "sim";
    const auto old = bytes(base / "freight/legacy134/g2.save");
    const auto refused = sim::World::load(old, ft::registry());
    REQUIRE_FALSE(refused);
    CHECK(refused.error() ==
          "save: state stream version 134 is not supported (this build reads 136)");
    auto log = sim::read_log(bytes(base / "expand1/current134/g2.log"));
    REQUIRE_FALSE(log);
    CHECK(log.error() == "stream (byte 24): replay state stream version differs from this build's");
    auto rec = sim::Recorder::start(w);
    REQUIRE(rec);
    rec.value().advance(0);
    rec.value().checkpoint();
    const auto good = rec.value().log();
    auto current = sim::World::load(good.initial_save, ft::registry());
    REQUIRE(current);
    const auto honest = sim::replay(current.value(), good);
    REQUIRE(honest);
    CHECK(honest.value().reproduced());
    auto nested = good;
    nested.initial_save = old;
    const auto before = w.save();
    const auto wrong = sim::replay(w, nested);
    REQUIRE_FALSE(wrong);
    CHECK(wrong.error() ==
          "replay refused: initial save state stream version differs from this build's");
    CHECK(w.save() == before);
}

TEST_CASE("freight state: every future field reaches hash and roundtrip and rules digest") {
    sim::FreightState base;
    base.stops = {{sim::BuildingId{1}, 10, 20, {{sim::FreightDirection::LoadTarget, 42, 2}}}};
    const auto hash = [](const sim::FreightState& r) {
        sim::HashArchive a;
        sim::freight_fields(a, r, 4);
        return a.finish();
    };
    const auto original = hash(base);
    const std::vector<std::function<void(sim::FreightState&)>> changes = {
        [](auto& r) { r.stops[0].warehouse = sim::BuildingId{2}; },
        [](auto& r) { ++r.stops[0].dock_x_q8; },
        [](auto& r) { ++r.stops[0].dock_y_q8; },
        [](auto& r) { r.stops[0].actions[0].direction = sim::FreightDirection::UnloadAmount; },
        [](auto& r) { ++r.stops[0].actions[0].goods; },
        [](auto& r) { ++r.stops[0].actions[0].tons; },
        [](auto& r) { r.stops.push_back(r.stops.front()); },
        [](auto& r) { r.status = sim::FreightStatus::Suspended; },
        [](auto& r) { r.phase = sim::FreightPhase::Wait; },
        [](auto& r) { ++r.stop_index; },
        [](auto& r) { ++r.action_index; },
        [](auto& r) { ++r.expected_order_id; },
        [](auto& r) { ++r.order_generation; },
        [](auto& r) { ++r.last_tick; },
        [](auto& r) { ++r.credit_ticks; },
        [](auto& r) { ++r.wait_remaining; },
        [](auto& r) { ++r.scan_next; },
        [](auto& r) { ++r.matching_raw; },
        [](auto& r) { ++r.remaining_raw; },
        [](auto& r) { r.matching_holds = {1}; },
        [](auto& r) { r.empty_holds = {2}; },
        [](auto& r) { ++r.candidate_next; },
        [](auto& r) { ++r.completed_cycles; },
        [](auto& r) { r.last_reject = sim::Reject::OutOfRange; },
        [](auto& r) { r.nav_failure = 1; }};
    CHECK(changes.size() == 25);
    for (const auto& change : changes) {
        auto r = base;
        change(r);
        CHECK(hash(r) != original);
        sim::WriteArchive a;
        sim::freight_fields(a, r, 4);
        sim::FreightState read;
        sim::ReadArchive b(a.data());
        sim::freight_fields(b, read, 4);
        REQUIRE_FALSE(b.failed());
        CHECK(b.remaining() == 0);
        CHECK(read == r);
    }
    const sim::Rules rules;
    for (unsigned field = 0; field < 3; ++field) {
        auto changed = rules;
        if (field == 0)
            ++changed.freight.max_stops;
        if (field == 1)
            ++changed.freight.max_actions;
        if (field == 2)
            ++changed.freight.work_per_visit;
        CHECK(changed.digest() != rules.digest());
        CHECK(changed.validate().empty());
    }
    auto w = ft::make();
    auto route = ft::route(w);
    auto& ship = ft::ship(w);
    ship.expiry_stamp = 0;
    const auto before = w.save();
    CHECK(w.apply({sim::PlayerId{0}, route}).status == sim::Reject::BadValue);
    CHECK(w.save() == before);
    ship.expiry_stamp.reset();
    REQUIRE(ft::apply(w, route).ok());
    sim::Effects fx;
    sim::end_figure(sim::WorldTestAccess::figures(w), ship, 0, fx);
    CHECK(w.freight_route(ft::ship_id) == nullptr);
    CHECK(w.check_invariants().empty());
    CHECK(sim::World::load(w.save(), ft::registry()).ok());
}
