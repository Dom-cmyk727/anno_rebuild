#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include <anno/sim/replay.hpp>
#include <anno/sim/ship_step.hpp>

#include "freight_helpers.hpp"
namespace sim = anno::sim;
namespace ft = freight_test;
namespace {
using D = sim::FreightDirection;
using C = sim::FreightControl;
using P = sim::FreightPhase;
using S = sim::FreightStatus;
std::map<std::pair<sim::PlayerId, sim::GoodsId>, std::uint64_t> balances(const sim::World& w) {
    std::map<std::pair<sim::PlayerId, sim::GoodsId>, std::uint64_t> out;
    for (const auto& island : w.islands())
        if (island)
            for (const auto& city : island->cities)
                if (city)
                    for (std::uint32_t g = 0; g < city->stock.stock.size(); ++g)
                        out[{city->owner, g}] += city->stock.stock[g];
    for (const auto& slot : w.figures().list.slots)
        if (slot)
            for (const auto& hold : slot->holds)
                if (hold.amount_88)
                    out[{slot->owner, hold.goods}] += hold.amount_88;
    return out;
}
std::uint32_t stock(const sim::World& w, unsigned island = 0, unsigned city = 0) {
    return w.islands()[island]->cities[city]->stock.stock[42];
}
sim::AssignFreightRouteCmd single(const sim::World& w, std::vector<sim::FreightAction> actions,
                                  sim::FigureId id = ft::ship_id) {
    const auto* ship = sim::find_figure(w.figures(), id);
    REQUIRE(ship);
    return {id, {{ft::warehouse(w), ship->pos.x, ship->pos.y, std::move(actions)}}};
}
void running(const sim::World& w, sim::FigureId id = ft::ship_id) {
    const auto* r = w.freight_route(id);
    REQUIRE(r);
    REQUIRE_MESSAGE(r->status == S::Running, sim::reject_name(r->last_reject));
}
void to_wait(sim::World& w) {
    for (unsigned i = 0; i < 30 && ft::live_route(w, ft::ship_id).phase != P::Wait; ++i) {
        w.advance(1000);
        running(w);
    }
    REQUIRE(ft::live_route(w, ft::ship_id).phase == P::Wait);
}
void to_cycle(sim::World& w, std::uint64_t cycles = 1) {
    const auto total = balances(w);
    for (unsigned i = 0; i < 1000 && ft::live_route(w, ft::ship_id).completed_cycles < cycles;
         ++i) {
        w.advance(1000);
        running(w);
        CHECK(balances(w) == total);
    }
    REQUIRE(ft::live_route(w, ft::ship_id).completed_cycles >= cycles);
}
void control(sim::World& w, C c) {
    REQUIRE(ft::apply(w, sim::ControlFreightRouteCmd{ft::ship_id, c}).ok());
}
} // namespace
TEST_CASE("freight lifecycle: arrival authority public replacement and stale revision predicate") {
    auto w = ft::make();
    REQUIRE(ft::apply(w, single(w, {{D::LoadTarget, 42, 1}})).ok());
    w.advance(2);
    running(w);
    REQUIRE(ft::live_route(w, ft::ship_id).phase == P::Sailing);
    const auto first = ft::live_route(w, ft::ship_id).expected_order_id;
    sim::SimEvent stale;
    stale.kind = sim::SimEventKind::ShipArrived;
    stale.figure = ft::ship_id;
    stale.order_id = first;
    CHECK(sim::FreightRoutes::matches_navigation(ft::ship(w), stale)); // legal identity control
    // Arrival becomes authoritative only when this mover emits its matching
    // event; no assumption about a typical navigation pickup interval is needed.
    bool arrived = false;
    for (unsigned i = 0; i < 20 && !arrived; ++i) {
        w.advance(2);
        running(w);
        for (const auto& e : w.drain_events())
            if (e.kind == sim::SimEventKind::ShipArrived && e.order_id == first)
                arrived = true;
        if (!arrived)
            CHECK(ft::live_route(w, ft::ship_id).phase == P::Sailing);
        CHECK(ft::ship(w).holds[0].amount_88 == 0);
    }
    REQUIRE(ft::apply(w, single(w, {{D::LoadTarget, 42, 2}})).ok());
    w.advance(2);
    const auto* r = w.freight_route(ft::ship_id);
    REQUIRE(r->phase == P::Sailing);
    CHECK(r->expected_order_id != first);
    CHECK_FALSE(sim::FreightRoutes::matches_navigation(ft::ship(w), stale));
    auto current = stale;
    current.order_id = r->expected_order_id;
    CHECK(sim::FreightRoutes::matches_navigation(ft::ship(w), current));
    auto other = current;
    other.figure = sim::FigureId{2};
    CHECK_FALSE(sim::FreightRoutes::matches_navigation(ft::ship(w), other));
    auto old_generation = ft::ship(w);
    ++old_generation.freight_generation;
    CHECK_FALSE(sim::FreightRoutes::matches_navigation(old_generation, current));
    control(w, C::Suspend);
    const auto suspended = w.save();
    CHECK_FALSE(sim::FreightRoutes::matches_navigation(ft::ship(w), current));
    for (unsigned i = 0; i < 3; ++i)
        w.advance(1000);
    CHECK(ft::live_route(w, ft::ship_id).phase == P::NeedLeg);
    CHECK(ft::ship(w).holds[0].amount_88 == 0);
    control(w, C::Resume);
    to_cycle(w);
    CHECK(ft::ship(w).holds[0].amount_88 == 512);
    CHECK(stock(w) == 20 * 256 - 512);
    CHECK(w.check_invariants().empty());
    CHECK(suspended != w.save());
}
TEST_CASE("freight lifecycle: pause resume native continuation and cancel after a partial commit") {
    ft::Setup setup;
    setup.capacity = 1024;
    setup.cargo_holds = 3;
    setup.work = 1;
    setup.initial_holds = {{42, 256}, {0, 0}, {42, 0}};
    setup.stock = {{42, 1024}};
    auto w = ft::make_with(setup);
    REQUIRE(ft::apply(w, single(w, {{D::LoadTarget, 42, 5}})).ok());
    for (unsigned i = 0; i < 80 && ft::live_route(w, ft::ship_id).candidate_next != 1; ++i) {
        w.advance(1000);
        running(w);
    }
    REQUIRE(ft::live_route(w, ft::ship_id).phase == P::Transfer);
    REQUIRE(ft::live_route(w, ft::ship_id).candidate_next == 1);
    REQUIRE(ft::ship(w).holds[0].amount_88 == 1024);
    const auto after = balances(w);
    const auto source = stock(w);
    control(w, C::Suspend);
    const auto cursor = ft::live_route(w, ft::ship_id).candidate_next;
    const auto credit = ft::live_route(w, ft::ship_id).credit_ticks;
    for (unsigned i = 0; i < 5; ++i)
        w.advance(1000);
    CHECK(balances(w) == after);
    CHECK(stock(w) == source);
    CHECK(ft::live_route(w, ft::ship_id).candidate_next == cursor);
    auto native = sim::World::load(w.save(), ft::registry(false, 1024, 3));
    REQUIRE(native);
    CHECK(ft::live_route(native.value(), ft::ship_id).candidate_next == 1); // lost-cursor fault
    auto cancelled = w;
    control(cancelled, C::Cancel);
    for (unsigned i = 0; i < 3; ++i)
        cancelled.advance(1000);
    CHECK(cancelled.freight_route(ft::ship_id) == nullptr);
    CHECK(stock(cancelled) == source);
    CHECK(ft::ship(cancelled).holds[0].amount_88 == 1024);
    CHECK(balances(cancelled) == after);
    auto replaced = w;
    const auto generation = ft::ship(replaced).freight_generation;
    REQUIRE(ft::apply(replaced, single(replaced, {{D::LoadTarget, 42, 4}})).ok());
    to_cycle(replaced);
    CHECK(ft::ship(replaced).freight_generation == generation + 1);
    CHECK(stock(replaced) == source);
    CHECK(ft::ship(replaced).holds[0].amount_88 == 1024);
    CHECK(balances(replaced) == after);
    control(w, C::Resume);
    control(native.value(), C::Resume);
    CHECK(ft::live_route(w, ft::ship_id).credit_ticks == credit);
    (void)w.drain_events();
    (void)native.value().drain_events();
    for (unsigned i = 0; i < 30 && ft::live_route(w, ft::ship_id).completed_cycles == 0; ++i) {
        w.advance(0);
        native.value().advance(0);
        CHECK(w.save() == native.value().save());
        CHECK(w.drain_events() == native.value().drain_events());
        CHECK(balances(w) == after);
    }
    REQUIRE(ft::live_route(w, ft::ship_id).completed_cycles == 1);
    CHECK(stock(w) == 0);
    CHECK(ft::ship(w).holds == std::vector<sim::ShipHold>{{42, 1024}, {0, 0}, {42, 256}});
    control(w, C::Cancel);
    CHECK(w.freight_route(ft::ship_id) == nullptr);
    const auto frozen = balances(w);
    for (unsigned i = 0; i < 5; ++i)
        w.advance(1000);
    CHECK(balances(w) == frozen);
}
TEST_CASE("freight authority: named city debits both owner ledgers and a closer warehouse cannot "
          "substitute") {
    for (const bool rival : {false, true}) {
        ft::Setup setup;
        setup.nearby_warehouse = true;
        setup.rival_nearby = rival;
        setup.shared_visibility = rival;
        setup.stock = {{42, 512}};
        auto w = ft::make_with(setup);
        const auto* near = w.building_at({sim::IslandId{0}, sim::CityNo{1}}, 106, 108);
        REQUIRE(near);
        const auto total = balances(w);
        const auto old_near = stock(w, 0, 1);
        auto route = single(w, {{D::LoadTarget, 42, 1}});
        if (rival) {
            auto wrong = route;
            wrong.stops[0].warehouse = near->id;
            const auto before = w.save();
            CHECK(ft::apply(w, wrong).status == sim::Reject::NotCityOwner);
            CHECK(w.save() == before);
        }
        REQUIRE(ft::apply(w, route).ok());
        to_cycle(w);
        CHECK(stock(w) == 256);
        CHECK(stock(w, 0, 1) == old_near);
        CHECK(ft::ship(w).holds[0].amount_88 == 256);
        CHECK(balances(w) == total);
        CHECK(w.players().find(sim::PlayerId{0})->money == 5000);
        CHECK(w.players().find(sim::PlayerId{1})->money == 5000);
    }
}
TEST_CASE("freight conservation: two ships and two owners contend in declared figure order") {
    ft::Setup setup;
    setup.ships = 3;
    setup.owners = {0, 0, 1};
    setup.stock = {{42, 512}};
    setup.rival_stock = {{42, 1024}};
    auto w = ft::make_with(setup);
    const auto total = balances(w);
    auto first = single(w, {{D::LoadTarget, 42, 2}});
    auto second = first;
    second.ship = sim::FigureId{2};
    auto other = single(w, {{D::LoadTarget, 42, 1}, {D::UnloadAmount, 42, 1}}, sim::FigureId{3});
    other.stops[0].warehouse = ft::warehouse(w, 2);
    REQUIRE(ft::apply(w, first).ok());
    REQUIRE(ft::apply(w, second).ok());
    REQUIRE(ft::apply(w, other, 1).ok());
    for (unsigned i = 0; i < 100 && (ft::live_route(w, ft::ship_id).completed_cycles == 0 ||
                                     ft::live_route(w, sim::FigureId{2}).completed_cycles == 0 ||
                                     ft::live_route(w, sim::FigureId{3}).completed_cycles == 0);
         ++i) {
        w.advance(1000);
        CHECK(balances(w) == total);
        running(w);
        running(w, sim::FigureId{2});
        running(w, sim::FigureId{3});
    }
    REQUIRE(ft::live_route(w, ft::ship_id).completed_cycles >= 1);
    REQUIRE(ft::live_route(w, sim::FigureId{2}).completed_cycles >= 1);
    REQUIRE(ft::live_route(w, sim::FigureId{3}).completed_cycles >= 1);
    CHECK(ft::ship(w).holds[0].amount_88 == 512);
    CHECK(ft::ship(w, sim::FigureId{2}).holds[0].amount_88 == 0);
    CHECK(stock(w) == 0);
    CHECK(stock(w, 2) == 1024);
    CHECK(ft::ship(w, sim::FigureId{3}).holds[0].amount_88 == 0);
    CHECK(w.check_invariants().empty());
}
TEST_CASE("freight recovery: shortage skips and a public cargo refill feeds a later cycle") {
    ft::Setup setup;
    setup.ships = 2;
    setup.stock = {{42, 0}};
    setup.holds_by_ship[1] = {{42, 512}, {0, 0}, {0, 0}, {0, 0}};
    auto w = ft::make_with(setup);
    const auto total = balances(w);
    REQUIRE(ft::apply(w, single(w, {{D::LoadTarget, 42, 2}})).ok());
    to_cycle(w);
    CHECK(ft::ship(w).holds[0].amount_88 == 0);
    const auto cycles = ft::live_route(w, ft::ship_id).completed_cycles;
    REQUIRE(ft::apply(w, sim::UnloadCmd{sim::FigureId{2}, ft::warehouse(w), 0, 42, 1}).ok());
    CHECK(balances(w) == total);
    for (unsigned i = 0; i < 50 && ft::ship(w).holds[0].amount_88 == 0; ++i) {
        w.advance(1000);
        running(w);
        CHECK(balances(w) == total);
    }
    CHECK(ft::ship(w).holds[0].amount_88 == 256);
    CHECK(stock(w) == 0);
    CHECK(ft::ship(w, sim::FigureId{2}).holds[0].amount_88 == 256);
    CHECK(ft::live_route(w, ft::ship_id).completed_cycles >= cycles);
}
TEST_CASE("freight failure: unreachable leg recovery uses reassignment and blocked cleanup stays "
          "public") {
    ft::Setup setup;
    setup.landlocked_goal = true;
    auto w = ft::make_with(setup);
    const auto total = balances(w);
    auto bad = single(w, {{D::LoadTarget, 42, 1}});
    bad.stops[0].dock_x_q8 = 410 * 256;
    bad.stops[0].dock_y_q8 = 410 * 256;
    REQUIRE(ft::apply(w, bad).ok());
    bool nav_failure = false;
    for (unsigned i = 0; i < 50 && ft::live_route(w, ft::ship_id).status == S::Running; ++i) {
        w.advance(1000);
        for (const auto& e : w.drain_events())
            if (e.kind == sim::SimEventKind::ShipRouteFailed)
                nav_failure = true;
        CHECK(balances(w) == total);
    }
    REQUIRE(nav_failure);
    CHECK(ft::live_route(w, ft::ship_id).status == S::Failed);
    CHECK(ft::live_route(w, ft::ship_id).phase == P::NeedLeg);
    CHECK(ft::live_route(w, ft::ship_id).nav_failure ==
          static_cast<std::uint8_t>(sim::NavFailReason::Unreachable));
    CHECK(ft::live_route(w, ft::ship_id).expected_order_id == 0);
    REQUIRE(ft::apply(w, single(w, {{D::LoadTarget, 42, 1}})).ok());
    to_cycle(w);
    CHECK(ft::ship(w).holds[0].amount_88 == 256);
    REQUIRE(ft::apply(w, single(w, {{D::LoadTarget, 42, 2}})).ok());
    w.advance(1000);
    REQUIRE(ft::live_route(w, ft::ship_id).phase == P::Sailing);
    ft::ship(w).state_word = 2; // simulated engine disability, not repaired for recovery
    for (unsigned i = 0; i < 5 && ft::live_route(w, ft::ship_id).status == S::Running; ++i)
        w.advance(1000);
    REQUIRE(ft::live_route(w, ft::ship_id).status == S::Failed);
    const auto before = w.save();
    CHECK(ft::apply(w, sim::ControlFreightRouteCmd{ft::ship_id, C::Resume}).status ==
          sim::Reject::ShipDisabled);
    CHECK(w.save() == before);
    control(w, C::Suspend);
    control(w, C::Cancel);
    CHECK(w.freight_route(ft::ship_id) == nullptr);
}
TEST_CASE("freight lifecycle: missing rebuilt target needs a new stable assignment and ending "
          "clears route") {
    ft::Setup setup;
    setup.stock[41] = 1280;
    setup.alternate = true;
    setup.land_warehouse = true; // data-defined buildable warehouse role
    auto w = ft::make_with(setup);
    REQUIRE(ft::apply(w, single(w, {{D::LoadTarget, 42, 1}})).ok());
    to_wait(w);
    const auto old = ft::warehouse(w);
    const sim::CityRef city{sim::IslandId{0}, sim::CityNo{0}};
    REQUIRE(ft::apply(w, sim::DemolishCmd{city, {{100, 108}}}).ok());
    w.advance(1000);
    REQUIRE(ft::live_route(w, ft::ship_id).status == S::Failed);
    CHECK(ft::live_route(w, ft::ship_id).last_reject == sim::Reject::NoBuildingAtTile);
    const auto unchanged = w.save();
    CHECK(ft::apply(w, sim::ControlFreightRouteCmd{ft::ship_id, C::Resume}).status ==
          sim::Reject::NoBuildingAtTile);
    CHECK(w.save() == unchanged);
    const auto construction =
        ft::apply(w, sim::BuildCmd{city, {{sim::TypeId{70001}, 100, 108, 0, 0, 0}}});
    REQUIRE_MESSAGE(construction.ok(), sim::reject_name(construction.status));
    CHECK(w.players().find(sim::PlayerId{0})->money == 4650);
    CHECK(w.islands()[0]->cities[0]->stock.stock[41] == 0);
    CHECK(stock(w) == 8 * 256); // 20t initial wood -12t normal warehouse bill
    const auto rebuilt = ft::warehouse(w);
    CHECK(rebuilt != old);
    CHECK(ft::apply(w, sim::ControlFreightRouteCmd{ft::ship_id, C::Resume}).status ==
          sim::Reject::NoBuildingAtTile);
    REQUIRE(ft::apply(w, single(w, {{D::LoadTarget, 42, 1}})).ok());
    to_cycle(w);
    CHECK(ft::ship(w).holds[0].amount_88 == 256);
    const auto source = stock(w);
    const auto total = balances(w);
    sim::Effects fx;
    sim::end_figure(sim::WorldTestAccess::figures(w), ft::ship(w),
                    static_cast<std::uint32_t>(w.clock().tick()), fx);
    CHECK(w.freight_route(ft::ship_id) == nullptr);
    w.advance(1000);
    CHECK(stock(w) == source);
    CHECK(balances(w) == total);
    const auto sink = ft::ship(w).holds[0].amount_88;
    w.advance(16000);
    CHECK(sim::find_figure(w.figures(), ft::ship_id) == nullptr);
    CHECK(stock(w) == source);
    CHECK(sink == 256);
    auto expected = total;
    expected[{sim::PlayerId{0}, 42}] -= sink;
    CHECK(balances(w) == expected); // explicit removed-unit cargo sink, no city credit
    CHECK(w.check_invariants().empty());
}

TEST_CASE("freight limits: full fractional destination and unavailable holds skip without "
          "inventing retries") {
    for (const std::uint32_t room : {0U, 100U}) {
        ft::Setup setup;
        setup.initial_holds = {{42, 512}, {0, 0}, {0, 0}, {0, 0}};
        setup.destination_stock = {{42, 12800 - room}};
        auto w = ft::make_with(setup);
        const auto total = balances(w);
        REQUIRE(ft::apply(w, ft::route(w)).ok());
        to_cycle(w);
        std::uint64_t loaded = 0, unloaded = 0;
        for (const auto& e : w.drain_events())
            if (e.freight && e.freight->signal == sim::FreightSignal::Transferred) {
                if (e.freight->direction == D::LoadTarget)
                    loaded += e.freight->amount_88;
                else
                    unloaded += e.freight->amount_88;
            }
        CHECK(loaded == 0);
        CHECK(unloaded == room);
        CHECK(stock(w, 1) == 12800);
        CHECK(ft::ship(w).holds[0].amount_88 == 512 - room);
        CHECK(balances(w) == total);
    }
    ft::Setup full;
    full.capacity = 512;
    full.cargo_holds = 2;
    full.initial_holds = {{42, 512}, {26, 512}};
    auto w = ft::make_with(full);
    const auto total = balances(w);
    REQUIRE(ft::apply(w, single(w, {{D::LoadTarget, 42, 5}, {D::LoadTarget, 91, 1}})).ok());
    to_cycle(w);
    CHECK(ft::ship(w).holds == full.initial_holds);
    CHECK(balances(w) == total);
    for (const auto& e : w.drain_events())
        if (e.freight)
            CHECK(e.freight->signal != sim::FreightSignal::Transferred);
}
TEST_CASE(
    "freight limits: exhausted cycle counter retries no cargo and public replacement recovers") {
    auto w = ft::make(1, false, 1);
    REQUIRE(ft::apply(w, single(w, {{D::LoadTarget, 42, 1}})).ok());
    for (unsigned i = 0; i < 80 && ft::live_route(w, ft::ship_id).phase != P::CompleteStop; ++i) {
        w.advance(1000);
        running(w);
    }
    REQUIRE(ft::live_route(w, ft::ship_id).phase == P::CompleteStop);
    REQUIRE(ft::ship(w).holds[0].amount_88 == 256);
    ft::ship(w).freight->completed_cycles = UINT64_MAX;
    auto valid = sim::World::load(w.save(), ft::registry());
    REQUIRE(valid);
    const auto total = balances(w);
    const auto source = stock(w);
    w.advance(0);
    REQUIRE(ft::live_route(w, ft::ship_id).status == S::Failed);
    CHECK(ft::live_route(w, ft::ship_id).last_reject == sim::Reject::SerialExhausted);
    control(w, C::Resume);
    w.advance(0);
    CHECK(ft::live_route(w, ft::ship_id).status == S::Failed);
    CHECK(stock(w) == source);
    CHECK(balances(w) == total);
    CHECK(ft::ship(w).holds[0].amount_88 == 256);
    REQUIRE(ft::apply(w, single(w, {{D::LoadTarget, 42, 1}})).ok());
    to_cycle(w);
    CHECK(stock(w) == source);
    CHECK(balances(w) == total);
}
TEST_CASE("freight lifecycle: attached crew follows automatic sailing and public interruption") {
    ft::Setup setup;
    setup.crew = true;
    auto w = ft::make_with(setup);
    REQUIRE(ft::apply(w, ft::route(w)).ok());
    bool interrupted = false, moved = false;
    const auto total = balances(w);
    const auto initial = ft::ship(w).pos;
    for (unsigned i = 0; i < 1000 && ft::live_route(w, ft::ship_id).completed_cycles == 0; ++i) {
        w.advance(1000);
        running(w);
        moved = moved || ft::ship(w).pos != initial;
        const auto* scout = sim::find_figure(w.figures(), sim::FigureId{2});
        const auto* donkey = sim::find_figure(w.figures(), sim::FigureId{3});
        REQUIRE(scout);
        REQUIRE(donkey);
        CHECK(scout->pos == ft::ship(w).pos);
        CHECK(donkey->pos == scout->pos);
        CHECK(scout->parent == ft::ship_id);
        CHECK(donkey->parent == sim::FigureId{2});
        CHECK(balances(w) == total);
        if (!interrupted && moved && ft::ship(w).holds[0].amount_88 == 512) {
            const auto before = w.save();
            CHECK(ft::apply(w, sim::MoveCmd{sim::FigureId{2}, 0, 0}).status ==
                  sim::Reject::NotAShip);
            CHECK(w.save() == before);
            control(w, C::Suspend);
            w.advance(1000);
            control(w, C::Resume);
            interrupted = true;
        }
    }
    REQUIRE(moved);
    REQUIRE(interrupted);
    REQUIRE(ft::live_route(w, ft::ship_id).completed_cycles == 1);
    CHECK(stock(w, 1) == 512);
    CHECK(balances(w) == total);
}

TEST_CASE("freight lifecycle: arrival and between-action waits pause resume cancel and reassign "
          "publicly") {
    auto w = ft::make(1, false, 8);
    REQUIRE(ft::apply(w, single(w, {{D::LoadTarget, 42, 1}, {D::UnloadAmount, 42, 1}})).ok());
    to_wait(w);
    REQUIRE(ft::live_route(w, ft::ship_id).action_index == 0);
    control(w, C::Suspend);
    for (unsigned i = 0; i < 3; ++i)
        w.advance(1000);
    CHECK(ft::ship(w).holds[0].amount_88 == 0);
    CHECK(ft::live_route(w, ft::ship_id).wait_remaining == 500);
    control(w, C::Resume);
    w.advance(1000);
    running(w);
    REQUIRE(ft::live_route(w, ft::ship_id).phase == P::Wait);
    REQUIRE(ft::live_route(w, ft::ship_id).action_index == 1);
    REQUIRE(ft::ship(w).holds[0].amount_88 == 256);
    control(w, C::Suspend);
    const auto total = balances(w);
    const auto source = stock(w);
    auto cancelled = w;
    control(cancelled, C::Cancel);
    for (unsigned i = 0; i < 3; ++i)
        cancelled.advance(1000);
    CHECK(stock(cancelled) == source);
    CHECK(ft::ship(cancelled).holds[0].amount_88 == 256);
    CHECK(balances(cancelled) == total);
    auto replaced = w;
    REQUIRE(ft::apply(replaced, single(replaced, {{D::UnloadAmount, 42, 1}})).ok());
    to_cycle(replaced);
    CHECK(stock(replaced) == 20 * 256);
    CHECK(ft::ship(replaced).holds[0].amount_88 == 0);
    CHECK(balances(replaced) == total);
    for (unsigned i = 0; i < 3; ++i)
        w.advance(1000);
    CHECK(stock(w) == source);
    control(w, C::Resume);
    to_cycle(w);
    CHECK(stock(w) == 20 * 256);
    CHECK(ft::ship(w).holds[0].amount_88 == 0);
    CHECK(balances(w) == total);
}

TEST_CASE("freight fault control: exact named warehouse debit and one-time target quantity") {
    ft::Setup setup;
    setup.nearby_warehouse = true;
    setup.stock = {{42, 512}};
    setup.nearby_stock = {{42, 1024}};
    auto w = ft::make_with(setup);
    const auto total = balances(w);
    REQUIRE(ft::apply(w, single(w, {{D::LoadTarget, 42, 1}})).ok());
    to_cycle(w);
    std::uint64_t actual = 0;
    unsigned effects = 0;
    for (const auto& e : w.drain_events())
        if (e.freight && e.freight->signal == sim::FreightSignal::Transferred) {
            REQUIRE(e.freight->warehouse == ft::warehouse(w));
            actual += e.freight->amount_88;
            ++effects;
        }
    CHECK(stock(w) == 256);
    CHECK(stock(w, 0, 1) == 1024);
    CHECK(ft::ship(w).holds[0].amount_88 == 256);
    CHECK(actual == 256);
    CHECK(effects == 1);
    CHECK(balances(w) == total);
}
