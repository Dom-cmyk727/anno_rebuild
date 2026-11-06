// R3: real scheduler visits/public commands, independent pre-C++ cargo answers.
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <anno/formats/json.hpp>
#include <anno/sim/replay.hpp>

#include "freight_helpers.hpp"
namespace sim = anno::sim;
namespace ft = freight_test;
namespace json = anno::formats::json;
namespace {
const json::Value& member(const json::Value& v, const char* key) {
    const auto* p = v.find(key);
    REQUIRE(p);
    return *p;
}
std::uint64_t num(const json::Value& v) {
    REQUIRE(v.kind == json::Kind::Integer);
    REQUIRE(v.integer >= 0);
    return static_cast<std::uint64_t>(v.integer);
}
std::uint32_t u32(const json::Value& v) {
    const auto n = num(v);
    REQUIRE(n <= UINT32_MAX);
    return static_cast<std::uint32_t>(n);
}
std::vector<sim::FreightAction> actions(const json::Value& v) {
    std::vector<sim::FreightAction> out;
    for (const auto& a : v.items)
        out.push_back({static_cast<sim::FreightDirection>(num(member(a, "direction"))),
                       u32(member(a, "goods")), num(member(a, "tons"))});
    return out;
}
std::vector<sim::ShipHold> holds(const json::Value& v) {
    std::vector<sim::ShipHold> out;
    for (const auto& a : v.items) {
        REQUIRE(a.items.size() == 2);
        out.push_back({u32(a.items[0]), num(a.items[1])});
    }
    return out;
}
std::uint32_t stock(const sim::World& w, unsigned island, sim::GoodsId goods) {
    return w.islands()[island]->cities[0]->stock.stock[goods];
}
void healthy(const sim::World& w) {
    REQUIRE_FALSE(w.faulted());
    const auto* r = w.freight_route(ft::ship_id);
    REQUIRE(r);
    INFO("pos " << sim::find_figure(w.figures(), ft::ship_id)->pos.x << ","
                << sim::find_figure(w.figures(), ft::ship_id)->pos.y << " phase "
                << static_cast<unsigned>(r->phase) << " tick " << w.clock().tick());
    REQUIRE_MESSAGE(r->status == sim::FreightStatus::Running, sim::reject_name(r->last_reject));
}
void wait_at_source(sim::World& w) {
    for (unsigned i = 0; i < 20; ++i) {
        const auto* r = w.freight_route(ft::ship_id);
        REQUIRE(r);
        if (r->phase == sim::FreightPhase::Wait)
            return;
        w.advance(1000);
        healthy(w);
    }
    FAIL("real arrival did not start wait");
}
sim::AssignFreightRouteCmd single(const sim::World& w, std::vector<sim::FreightAction> a) {
    auto r = ft::route(w);
    r.stops.resize(1);
    r.stops[0].actions = std::move(a);
    return r;
}
} // namespace
TEST_CASE("freight visits: nineteen independent target grouping rounding and hold-prefix vectors") {
    std::ifstream in(std::filesystem::path(ANNO_FIXTURES_DIR) /
                     "sim/freight/contract_vectors.jsonl");
    REQUIRE(in.good());
    unsigned executed = 0;
    for (std::string line; std::getline(in, line);) {
        auto parsed =
            json::parse({reinterpret_cast<const std::uint8_t*>(line.data()), line.size()});
        REQUIRE(parsed);
        const auto& row = parsed.value();
        if (member(row, "kind").text != "cargo")
            continue;
        INFO(member(row, "name").text);
        ++executed;
        const auto& input = member(row, "input");
        const auto& expected = member(row, "expect");
        ft::Setup setup;
        setup.capacity = u32(member(input, "hold_capacity"));
        setup.cargo_holds = u32(member(input, "hold_count_a"));
        setup.store_capacity = u32(member(input, "store_capacity"));
        setup.initial_holds = holds(member(input, "holds"));
        setup.stock.clear();
        for (const auto& [key, value] : member(input, "stock").members)
            setup.stock[static_cast<std::uint32_t>(std::stoul(key))] = u32(value);
        auto w = ft::make_with(setup);
        const auto route = single(w, actions(member(input, "actions")));
        REQUIRE(ft::apply(w, route).ok());
        std::vector<std::pair<std::uint32_t, std::uint64_t>> effects;
        unsigned calls = 0;
        while (w.freight_route(ft::ship_id)->completed_cycles == 0 && calls++ < 100) {
            w.advance(1000);
            healthy(w);
            for (const auto& event : w.drain_events())
                if (event.freight && event.freight->signal == sim::FreightSignal::Transferred)
                    effects.emplace_back(event.freight->action, event.freight->amount_88);
        }
        REQUIRE(calls < 100);
        CHECK(ft::ship(w).holds == holds(member(expected, "holds")));
        for (const auto& [key, value] : member(expected, "stock").members)
            CHECK(stock(w, 0, static_cast<sim::GoodsId>(std::stoul(key))) == num(value));
        CHECK(w.freight_route(ft::ship_id)->stops[0].actions ==
              actions(member(expected, "actions")));
        std::vector<std::pair<std::uint32_t, std::uint64_t>> wanted;
        for (const auto& effect : member(expected, "effects").items)
            if (num(effect.items[3]) != 0)
                wanted.emplace_back(u32(effect.items[0]), num(effect.items[3]));
        CHECK(effects == wanted);
        CHECK(w.check_invariants().empty());
        CHECK(w.freight_work().steps <= w.freight_work().visits * w.rules().freight.work_per_visit);
        CHECK(w.freight_work().cold_scans == 0);
    }
    CHECK(executed == 19);
}
TEST_CASE("freight timing: actual arrival zero smallest split surplus and quota continuation") {
    using D = sim::FreightDirection;
    auto w = ft::make();
    REQUIRE(ft::apply(w, single(w, {{D::LoadTarget, 42, 1}})).ok());
    wait_at_source(w);
    REQUIRE(w.figures().list.slots.size() == 1);
    const auto t = w.clock().tick();
    CHECK(w.freight_route(ft::ship_id)->wait_remaining == 500);
    auto whole = w;
    w.advance(0);
    CHECK(w.clock().tick() == t);
    CHECK(w.freight_route(ft::ship_id)->wait_remaining == 500);
    w.advance(1);
    CHECK(w.freight_route(ft::ship_id)->wait_remaining == 500);
    w.advance(1);
    CHECK(w.freight_route(ft::ship_id)->wait_remaining == 499);
    w.advance(996);
    CHECK(w.freight_route(ft::ship_id)->wait_remaining == 1);
    CHECK(ft::ship(w).holds[0].amount_88 == 0);
    w.advance(2);
    whole.advance(1000);
    CHECK(ft::ship(w).holds == ft::ship(whole).holds);
    CHECK(*w.freight_route(ft::ship_id) == *whole.freight_route(ft::ship_id));
    CHECK(ft::ship(w).holds[0].amount_88 == 256);
    auto quota = ft::make(1, false, 8);
    REQUIRE(
        ft::apply(quota, single(quota, {{D::LoadTarget, 42, 1}, {D::UnloadAmount, 42, 1}})).ok());
    wait_at_source(quota);
    quota.advance(2500);
    healthy(quota);
    const auto* r = quota.freight_route(ft::ship_id);
    CHECK(r->phase == sim::FreightPhase::Wait);
    CHECK(r->action_index == 1);
    CHECK(r->wait_remaining == 500);
    CHECK(r->credit_ticks == 750);
    auto reloaded = sim::World::load(quota.save(), ft::registry());
    REQUIRE(reloaded);
    quota.advance(0);
    reloaded.value().advance(0);
    CHECK(quota.save() == reloaded.value().save());
    r = quota.freight_route(ft::ship_id);
    CHECK(r->phase == sim::FreightPhase::CompleteStop);
    CHECK(r->credit_ticks == 250);
    quota.advance(0);
    r = quota.freight_route(ft::ship_id);
    CHECK(r->completed_cycles == 1);
    CHECK(r->phase == sim::FreightPhase::Sailing);
    CHECK(r->credit_ticks == 0);
    CHECK(ft::ship(quota).holds[0].amount_88 == 0);
    CHECK(stock(quota, 0, 42) == 20 * 256);
}
TEST_CASE("freight sailing: automatic matching arrivals deliver twice and remote warehouse range "
          "refuses") {
    auto w = ft::make();
    const auto before = stock(w, 0, 42);
    REQUIRE(ft::apply(w, ft::route(w)).ok());
    CHECK_FALSE(ft::ship(w).nav->order);
    CHECK(w.freight_route(ft::ship_id)->phase == sim::FreightPhase::NeedLeg);
    std::uint64_t loads = 0, unloads = 0;
    unsigned arrivals = 0;
    bool moved = false;
    for (unsigned i = 0; i < 1000 && w.freight_route(ft::ship_id)->completed_cycles < 2; ++i) {
        const auto pos = ft::ship(w).pos;
        w.advance(1000);
        healthy(w);
        moved = moved || ft::ship(w).pos != pos;
        for (const auto& e : w.drain_events()) {
            if (e.kind == sim::SimEventKind::ShipArrived)
                ++arrivals;
            if (e.freight && e.freight->signal == sim::FreightSignal::Transferred) {
                if (e.freight->direction == sim::FreightDirection::LoadTarget)
                    loads += e.freight->amount_88;
                else
                    unloads += e.freight->amount_88;
            }
        }
    }
    REQUIRE(w.freight_route(ft::ship_id)->completed_cycles == 2);
    CHECK(moved);
    CHECK(arrivals >= 4);
    CHECK(loads == 1024);
    CHECK(unloads == 1024);
    CHECK(stock(w, 0, 42) == before - 1024);
    CHECK(stock(w, 1, 42) == 1024);
    CHECK(ft::ship(w).holds[0].amount_88 == 0);
    CHECK(w.check_invariants().empty());
    auto far = ft::make();
    auto bad = single(far, {{sim::FreightDirection::LoadTarget, 42, 2}});
    bad.stops[0].dock_x_q8 = 65 * 256;
    REQUIRE(ft::apply(far, bad).ok());
    bool sailed = false;
    const auto original_pos = ft::ship(far).pos;
    for (unsigned i = 0;
         i < 500 && far.freight_route(ft::ship_id)->status == sim::FreightStatus::Running; ++i) {
        far.advance(1000);
        sailed = sailed || ft::ship(far).pos != original_pos;
        if (far.freight_route(ft::ship_id)->phase == sim::FreightPhase::Sailing)
            CHECK(ft::ship(far).holds[0].amount_88 == 0);
    }
    const auto* failed = far.freight_route(ft::ship_id);
    REQUIRE(failed);
    CHECK(sailed);
    CHECK(failed->status == sim::FreightStatus::Failed);
    CHECK(failed->last_reject == sim::Reject::OutOfRange);
    CHECK(failed->completed_cycles == 0);
    CHECK(stock(far, 0, 42) == 20 * 256);
    CHECK(ft::ship(far).holds[0].amount_88 == 0);
    CHECK(far.check_invariants().empty());
}
TEST_CASE("freight continuation: public partial effect reload completes exact remaining suffix") {
    ft::Setup setup;
    setup.capacity = 1024;
    setup.cargo_holds = 3;
    setup.work = 1;
    setup.initial_holds = {{91, 256}, {0, 0}, {91, 0}};
    setup.stock = {{91, 1024}};
    auto w = ft::make_with(setup);
    REQUIRE(ft::apply(w, single(w, {{sim::FreightDirection::LoadTarget, 91, 5}})).ok());
    for (unsigned i = 0; i < 80 && w.freight_route(ft::ship_id)->candidate_next != 1; ++i) {
        w.advance(1000);
        healthy(w);
    }
    const auto* partial = w.freight_route(ft::ship_id);
    REQUIRE(partial->phase == sim::FreightPhase::Transfer);
    REQUIRE(partial->candidate_next == 1);
    CHECK(partial->remaining_raw == 256);
    CHECK(partial->matching_raw == 256);
    CHECK(ft::ship(w).holds[0].amount_88 == 1024);
    (void)w.drain_events();
    auto native = sim::World::load(w.save(), ft::registry(false, 1024, 3));
    REQUIRE(native);
    CHECK(native.value().freight_work().cold_scans == 3);
    unsigned steps = 0;
    while (w.freight_route(ft::ship_id)->completed_cycles == 0 && steps++ < 80) {
        w.advance(2);
        native.value().advance(2);
        healthy(w);
        healthy(native.value());
        CHECK(w.save() == native.value().save());
        CHECK(w.drain_events() == native.value().drain_events());
        CHECK(w.rng().cursor() == native.value().rng().cursor());
    }
    REQUIRE(steps < 80);
    CHECK(ft::ship(w).holds == std::vector<sim::ShipHold>{{91, 1024}, {0, 0}, {91, 256}});
    CHECK(stock(w, 0, 91) == 0);
    CHECK(w.check_invariants().empty());
}
TEST_CASE("freight work: colocated empty and resource-refused routes cannot spin") {
    for (const bool empty : {false, true}) {
        auto w = ft::make(1, false, 3);
        auto route = single(w, {});
        if (!empty)
            route.stops[0].actions = {{sim::FreightDirection::UnloadAmount, 42, 2}};
        REQUIRE(ft::apply(w, route).ok());
        for (unsigned i = 0; i < 30; ++i) {
            w.advance(2);
            healthy(w);
            const auto& work = w.freight_work();
            CHECK(work.steps <= work.visits * 3);
            CHECK(w.freight_route(ft::ship_id)->completed_cycles <= work.visits);
        }
        CHECK(stock(w, 0, 42) == 20 * 256);
        CHECK(ft::ship(w).holds[0].amount_88 == 0);
    }
}

TEST_CASE("freight ingress: same-tick cancellation replacement and refused manual commands precede "
          "visits") {
    using D = sim::FreightDirection;
    using C = sim::FreightControl;
    auto start = ft::make();
    REQUIRE(ft::apply(start, single(start, {{D::LoadTarget, 42, 1}})).ok());
    wait_at_source(start);
    const auto old_generation = ft::ship(start).freight_generation;
    auto cancelled = start;
    sim::Turn turn;
    turn.step_ms = 1000;
    turn.commands = {{sim::PlayerId{0}, sim::ControlFreightRouteCmd{ft::ship_id, C::Cancel}}};
    std::vector<sim::ApplyResult> results;
    REQUIRE(sim::run_turn(cancelled, nullptr, turn, 1, &results).empty());
    REQUIRE(results.size() == 1);
    CHECK(results[0].ok());
    CHECK(cancelled.freight_route(ft::ship_id) == nullptr);
    CHECK(ft::ship(cancelled).holds[0].amount_88 == 0);
    CHECK(stock(cancelled, 0, 42) == 20 * 256);
    auto replaced = start;
    turn.commands = {{sim::PlayerId{0}, single(replaced, {{D::LoadTarget, 42, 2}})}};
    results.clear();
    REQUIRE(sim::run_turn(replaced, nullptr, turn, 1, &results).empty());
    CHECK(results[0].ok());
    CHECK(ft::ship(replaced).freight_generation == old_generation + 1);
    CHECK(replaced.freight_route(ft::ship_id)->phase == sim::FreightPhase::Sailing);
    CHECK(ft::ship(replaced).holds[0].amount_88 == 0); // old due action did not execute
    auto refused = start;
    turn.commands = {{sim::PlayerId{0}, sim::MoveCmd{ft::ship_id, -1, 0}}};
    results.clear();
    REQUIRE(sim::run_turn(refused, nullptr, turn, 1, &results).empty());
    CHECK(results[0].status == sim::Reject::BadValue);
    CHECK(ft::ship(refused).freight_generation == old_generation);
    CHECK(ft::ship(refused).holds[0].amount_88 == 256);
    auto normal = start;
    normal.advance(1000);
    const auto committed = stock(normal, 0, 42);
    REQUIRE(ft::apply(normal, sim::ControlFreightRouteCmd{ft::ship_id, C::Cancel}).ok());
    CHECK(stock(normal, 0, 42) == committed);
    CHECK(ft::ship(normal).holds[0].amount_88 == 256); // cannot undo earlier commit
}
