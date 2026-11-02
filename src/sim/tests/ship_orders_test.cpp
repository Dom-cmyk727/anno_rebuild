// MoveCmd / StopCmd, the mover and the fair planning admission (task MAP-NAV-2 package E; contract
// docs/sim/ship-navigation.md 2, 12.1, 12.2). Synthetic: an open 512 x 512 sea, no game files.
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/route.hpp>
#include <anno/sim/route_world.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/ship.hpp>
#include <anno/sim/ship_mover.hpp>
#include <anno/sim/ship_step.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/world.hpp>

#include "island_helpers.hpp"
#include "ship_helpers.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;
using ship_test::kDonkey;
using ship_test::kScout;
using ship_test::kShip;

namespace {

constexpr std::int32_t kTile = 256;

std::int32_t tile_c(int t) { // the centre of a tile in Q24.8
    return t * kTile + kTile / 2;
}

struct Spec {
    std::uint32_t ships = 1;
    std::uint32_t scouts = 0; // plain figures after the ships (registry filler)
    std::uint32_t players = 2;
    bool crew = false;                  // a scout and a donkey aboard ship 1
    std::uint32_t first_four_owner = 0; // the owner of ships 1..4 (the others belong to player 0)
};

sim::ScenarioSpec spec_of(const Spec& s) {
    sim::ScenarioSpec spec = syn::empty_scenario(11);
    for (std::uint32_t p = 0; p < s.players; ++p) {
        sim::ScenarioSpec::PlayerSpec ps;
        ps.id = sim::PlayerId{p};
        ps.money = 1000;
        spec.players.push_back(ps);
    }
    for (std::uint32_t i = 0; i < s.ships; ++i) {
        sim::ScenarioSpec::FigureSpec f;
        f.kind = kShip;
        f.owner = sim::PlayerId{i < 4 ? s.first_four_owner : 0};
        f.pos = sim::FixedPos{tile_c(20), tile_c(64 + 3 * static_cast<int>(i))};
        f.dir16 = 8;
        spec.figures.push_back(f);
    }
    if (s.crew) {
        sim::ScenarioSpec::FigureSpec scout = spec.figures[0];
        scout.kind = kScout;
        scout.parent = 0;
        scout.crew_slot = 0;
        spec.figures.push_back(scout);
        sim::ScenarioSpec::FigureSpec donkey = scout;
        donkey.kind = kDonkey;
        donkey.parent = static_cast<std::uint32_t>(spec.figures.size() - 1);
        donkey.crew_slot.reset();
        spec.figures.push_back(donkey);
    }
    for (std::uint32_t i = 0; i < s.scouts; ++i) {
        sim::ScenarioSpec::FigureSpec f;
        f.kind = kScout;
        f.owner = sim::PlayerId{0};
        f.pos = sim::FixedPos{tile_c(300), tile_c(300)};
        spec.figures.push_back(f);
    }
    return spec;
}

sim::World make_world(const Spec& s, const sim::Executor& ex = sim::serial_executor()) {
    auto w = sim::World::create(spec_of(s), ship_test::nav_ship_registry(), {}, {}, {}, ex);
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    return std::move(w).value();
}

sim::Figure& ship(sim::World& w, std::uint64_t id) {
    sim::Figure* f = sim::find_figure(sim::WorldTestAccess::figures(w), sim::FigureId{id});
    REQUIRE(f != nullptr);
    return *f;
}

sim::Command move(std::uint32_t issuer, std::uint64_t id, int tx, int ty) {
    return sim::Command{sim::PlayerId{issuer},
                        sim::MoveCmd{sim::FigureId{id}, tile_c(tx), tile_c(ty)}};
}

sim::Command stop(std::uint32_t issuer, std::uint64_t id) {
    return sim::Command{sim::PlayerId{issuer}, sim::StopCmd{sim::FigureId{id}}};
}

std::vector<sim::SimEvent> events_of(sim::World& w, sim::SimEventKind kind) {
    std::vector<sim::SimEvent> out;
    for (const sim::SimEvent& e : w.drain_events()) {
        if (e.kind == kind) {
            out.push_back(e);
        }
    }
    return out;
}

} // namespace

TEST_CASE(
    "ship commands: the wire bytes equal the hand transcription, malformed records are refused") {
    // MoveCmd {ship 0x0000000100000002, target (-3, 0x1234)}: type F1 F0 | len 10 00 |
    // ship u64 = 02 00 00 00 01 00 00 00 | x i32 = FD FF FF FF | y = 34 12 00 00
    const sim::Body mv = sim::MoveCmd{sim::FigureId{0x0000000100000002ULL}, -3, 0x1234};
    auto bytes = sim::encode_record(mv);
    REQUIRE(bytes.ok());
    const std::vector<std::uint8_t> want_move = {0x71, 0xF0, 0x10, 0x00, 0x02, 0x00, 0x00,
                                                 0x00, 0x01, 0x00, 0x00, 0x00, 0xFD, 0xFF,
                                                 0xFF, 0xFF, 0x34, 0x12, 0x00, 0x00};
    CHECK(bytes.value() == want_move);
    auto back = sim::decode_record(want_move);
    REQUIRE(back.ok());
    CHECK(back.value() == mv);
    // StopCmd {ship 7}: type 72 F0 | len 08 00 | 07 00 00 00 00 00 00 00
    const sim::Body st = sim::StopCmd{sim::FigureId{7}};
    const std::vector<std::uint8_t> want_stop = {0x72, 0xF0, 0x08, 0x00, 0x07, 0x00,
                                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    auto sbytes = sim::encode_record(st);
    REQUIRE(sbytes.ok());
    CHECK(sbytes.value() == want_stop);
    CHECK(sim::decode_record(want_stop).value() == st);
    // A (0, 0) target is a valid MoveCmd, distinct from a StopCmd (12.1).
    CHECK(sim::encode_record(sim::Body{sim::MoveCmd{sim::FigureId{7}, 0, 0}}).value() != want_stop);
    // Malformed: one byte short or long, trailing byte, a header only, a payload of the other kind.
    for (std::size_t cut : {std::size_t{1}, std::size_t{4}}) {
        std::vector<std::uint8_t> shorter(want_move.begin(),
                                          want_move.end() - static_cast<long>(cut));
        CHECK_FALSE(sim::decode_record(shorter).ok());
    }
    std::vector<std::uint8_t> longer = want_move;
    longer.push_back(0);
    CHECK_FALSE(sim::decode_record(longer).ok());
    std::vector<std::uint8_t> wrong_len = want_move; // length field says 15, payload has 16
    wrong_len[2] = 0x0F;
    CHECK_FALSE(sim::decode_record(wrong_len).ok());
    std::vector<std::uint8_t> stop_as_move = want_stop;
    stop_as_move[0] = 0x71;
    CHECK_FALSE(sim::decode_record(stop_as_move).ok()); // 8 bytes where a move needs 16
    std::vector<std::uint8_t> move_as_stop = want_move;
    move_as_stop[0] = 0x72;
    CHECK_FALSE(sim::decode_record(move_as_stop).ok());
    CHECK_FALSE(sim::decode_record(std::vector<std::uint8_t>{0x71, 0xF0}).ok());
    // The tags are native: a LegacyRawCmd of such a type is unrepresentable.
    CHECK(sim::is_native_wire_type(0xF071));
    CHECK(sim::is_native_wire_type(0xF072));
    CHECK_FALSE(sim::has_wire_form(sim::Body{sim::LegacyRawCmd{0xF071, {1, 2}}}));
    CHECK_FALSE(sim::has_wire_form(sim::Body{sim::LegacyRawCmd{0xF072, {}}}));
    // The turn entry round trip keeps the issuer and the order.
    sim::TurnEntry te{sim::Command{sim::PlayerId{3}, mv}, 9};
    auto enc = sim::encode_turn_entry(te);
    REQUIRE(enc.ok());
    CHECK(sim::decode_turn_entry(enc.value()).value() == te);
    // The variant order is part of the replay format: Move and Stop follow Found.
    CHECK(sim::Body{sim::FoundCmd{}}.index() == 7);
    CHECK(mv.index() == 8);
    CHECK(st.index() == 9);
}

TEST_CASE("ship commands: every refusal is atomic, the valid control is accepted") {
    sim::World w = make_world(Spec{.ships = 1, .scouts = 0, .players = 2, .crew = true});
    // ids: ship 1, scout 2 (aboard), donkey 3
    const std::vector<std::uint8_t> before = w.save();
    const std::uint64_t hash = w.state_hash();
    struct Refusal {
        const char* what;
        sim::Command command;
        sim::Reject want;
    };
    const std::vector<Refusal> refusals = {
        {"unknown figure", move(0, 99, 30, 64), sim::Reject::UnknownFigure},
        {"not a ship (the scout)", move(0, 2, 30, 64), sim::Reject::NotAShip},
        {"another player's ship", move(1, 1, 30, 64), sim::Reject::NotShipOwner},
        {"unknown issuer", move(9, 1, 30, 64), sim::Reject::UnknownIssuer},
        {"x below the world",
         sim::Command{sim::PlayerId{0}, sim::MoveCmd{sim::FigureId{1}, -1, 5000}},
         sim::Reject::BadValue},
        {"y below the world",
         sim::Command{sim::PlayerId{0}, sim::MoveCmd{sim::FigureId{1}, 5000, -1}},
         sim::Reject::BadValue},
        {"x at the world edge",
         sim::Command{sim::PlayerId{0}, sim::MoveCmd{sim::FigureId{1}, 512 * 256, 5000}},
         sim::Reject::BadValue},
        {"y at the world edge",
         sim::Command{sim::PlayerId{0}, sim::MoveCmd{sim::FigureId{1}, 5000, 512 * 256}},
         sim::Reject::BadValue},
        {"stop: unknown figure", stop(0, 99), sim::Reject::UnknownFigure},
        {"stop: not a ship", stop(0, 3), sim::Reject::NotAShip},
        {"stop: another player's ship", stop(1, 1), sim::Reject::NotShipOwner},
    };
    for (const Refusal& r : refusals) {
        INFO(r.what);
        const sim::ApplyResult res = w.apply(r.command);
        CHECK(res.status == r.want);
        CHECK(w.save() == before);
        CHECK(w.state_hash() == hash);
    }
    // A blocked ship (state bit 0x2 is in the mask 0x200e) refuses both commands.
    {
        sim::World b = w;
        ship(b, 1).state_word = 0x2;
        const std::vector<std::uint8_t> bb = b.save();
        CHECK(b.apply(move(0, 1, 30, 64)).status == sim::Reject::ShipDisabled);
        CHECK(b.apply(stop(0, 1)).status == sim::Reject::ShipDisabled);
        CHECK(b.save() == bb);
        ship(b, 1).state_word = 0x1; // moving only: accepted (0x1 is not in the mask)
        CHECK(b.apply(move(0, 1, 30, 64)).ok());
    }
    // A ship that is itself a crew child takes no orders.
    {
        sim::World b = w;
        ship(b, 1).parent = sim::FigureId{2};
        const std::vector<std::uint8_t> bb = b.save();
        CHECK(b.apply(move(0, 1, 30, 64)).status == sim::Reject::ShipEmbarked);
        CHECK(b.apply(stop(0, 1)).status == sim::Reject::ShipEmbarked);
        CHECK(b.save() == bb);
    }
    // Valid controls: the corner (0, 0) and the last Q24.8 unit of the world are accepted.
    {
        sim::World b = w;
        CHECK(b.apply(sim::Command{sim::PlayerId{0}, sim::MoveCmd{sim::FigureId{1}, 0, 0}}).ok());
        CHECK(b.apply(sim::Command{sim::PlayerId{0},
                                   sim::MoveCmd{sim::FigureId{1}, 512 * 256 - 1, 512 * 256 - 1}})
                  .ok());
        CHECK(b.check_invariants().empty());
    }
}

TEST_CASE("ship commands: order identity, ticket kept on replacement, stop clears both") {
    sim::World w = make_world(Spec{.ships = 2, .players = 2});
    CHECK(w.ids().next_order_id == 1);
    CHECK(w.ids().next_ticket == 1);
    REQUIRE(w.apply(move(0, 1, 30, 64)).ok());
    REQUIRE(w.apply(move(0, 2, 30, 67)).ok());
    CHECK(ship(w, 1).nav->order->order_id == 1);
    CHECK(ship(w, 2).nav->order->order_id == 2);
    CHECK(ship(w, 1).nav->ticket == std::optional<std::uint64_t>(1));
    CHECK(ship(w, 2).nav->ticket == std::optional<std::uint64_t>(2));
    CHECK_FALSE(ship(w, 1).nav->order->routed);
    CHECK(w.ids().next_order_id == 3);
    CHECK(w.ids().next_ticket == 3);
    // Replacement: a new order identity, the same ticket (its place in the queue).
    REQUIRE(w.apply(move(0, 1, 40, 70)).ok());
    CHECK(ship(w, 1).nav->order->order_id == 3);
    CHECK(ship(w, 1).nav->order->target_x_q8 == tile_c(40));
    CHECK(ship(w, 1).nav->ticket == std::optional<std::uint64_t>(1));
    CHECK(w.ids().next_ticket == 3);
    // Two commands of the same tick: the later wins, the ticket stays.
    REQUIRE(w.apply(move(0, 1, 41, 70)).ok());
    REQUIRE(w.apply(move(0, 1, 42, 70)).ok());
    CHECK(ship(w, 1).nav->order->order_id == 5);
    CHECK(ship(w, 1).nav->order->target_x_q8 == tile_c(42));
    CHECK(ship(w, 1).nav->ticket == std::optional<std::uint64_t>(1));
    CHECK(w.check_invariants().empty());
    // Stop: order and ticket gone, one ShipStopped event for the cancelled order.
    (void)w.drain_events();
    REQUIRE(w.apply(stop(0, 1)).ok());
    CHECK_FALSE(ship(w, 1).nav->order.has_value());
    CHECK_FALSE(ship(w, 1).nav->ticket.has_value());
    const auto stopped = events_of(w, sim::SimEventKind::ShipStopped);
    REQUIRE(stopped.size() == 1);
    CHECK(stopped[0].order_id == 5);
    CHECK(stopped[0].figure == sim::FigureId{1});
    // A stop of an idle ship is a no-op that emits nothing; the counters do not move.
    const std::uint64_t orders = w.ids().next_order_id;
    REQUIRE(w.apply(stop(0, 1)).ok());
    CHECK(events_of(w, sim::SimEventKind::ShipStopped).empty());
    CHECK(w.ids().next_order_id == orders);
    // Ship 2's pending order is untouched by the stop of ship 1.
    CHECK(ship(w, 2).nav->order->order_id == 2);
    CHECK(w.check_invariants().empty());
}

TEST_CASE("sailing: the ship really sails an open-sea route, no teleport, arrives with its event") {
    sim::World w = make_world(Spec{.ships = 1});
    const sim::FixedPos start = ship(w, 1).pos;
    REQUIRE(w.apply(move(0, 1, 70, 64)).ok());
    (void)w.drain_events();
    std::int32_t worst_step = 0;
    std::int32_t last_x = start.x;
    std::uint64_t arrived_at = 0;
    bool routed_seen = false;
    std::vector<sim::SimEvent> arrivals;
    for (int i = 0; i < 2600 && arrived_at == 0; ++i) {
        w.advance(10); // 10 ms = 5 ticks at 100 percent
        const sim::Figure& f = ship(w, 1);
        worst_step = std::max(worst_step, std::abs(f.pos.x - last_x));
        last_x = f.pos.x;
        routed_seen = routed_seen || (f.nav->order.has_value() && f.nav->order->routed);
        for (const sim::SimEvent& e : w.drain_events()) {
            if (e.kind == sim::SimEventKind::ShipArrived) {
                arrivals.push_back(e);
                arrived_at = w.clock().tick();
            }
        }
        REQUIRE(w.check_invariants().empty());
    }
    CHECK(routed_seen);
    REQUIRE(arrivals.size() == 1);
    CHECK(arrivals[0].order_id == 1);
    CHECK(arrivals[0].figure == sim::FigureId{1});
    CHECK_FALSE(ship(w, 1).nav->order.has_value());
    const sim::Figure& f = ship(w, 1);
    // One advance (5 ticks = 1 stepper unit) moves at most 0.036 tile = 9.2 Q24.8 units: 10 covers
    // it; a teleport of the whole route would show up as a step of thousands.
    CHECK(worst_step <= 10);
    // The voyage is about 50 tiles: it took about 50 / 0.036 = 1389 stepper units (advances).
    CHECK(w.clock().tick() > 6000);
    CHECK(w.clock().tick() < 8000);
    // The ship stands on the goal tile (open sea: the route is straight east).
    CHECK(f.pos.x / kTile == 70);
    CHECK(f.pos.y / kTile == 64);
    CHECK((f.state_word & 1U) == 0);
}

TEST_CASE("sailing: a diagonal and bending route follows the lattice, the crew rides along") {
    sim::World w = make_world(Spec{.ships = 1, .crew = true});
    REQUIRE(w.apply(move(0, 1, 50, 90)).ok()); // 30 east, 26 south: diagonal then straight
    bool arrived = false;
    for (int i = 0; i < 3000 && !arrived; ++i) {
        w.advance(10);
        for (const sim::SimEvent& e : w.drain_events()) {
            arrived = arrived || e.kind == sim::SimEventKind::ShipArrived;
        }
        // The scout and the donkey are written from the ship every visit.
        if (i % 7 == 0 || arrived) {
            const sim::Figure& s = ship(w, 1);
            CHECK(ship(w, 2).pos.x == s.pos.x);
            CHECK(ship(w, 2).pos.y == s.pos.y);
            CHECK(ship(w, 3).pos.x == s.pos.x);
            CHECK(ship(w, 3).pos.y == s.pos.y);
        }
    }
    REQUIRE(arrived);
    CHECK(ship(w, 1).pos.x / kTile == 50);
    CHECK(ship(w, 1).pos.y / kTile == 90);
    CHECK(ship(w, 2).pos.x == ship(w, 1).pos.x);
    CHECK(w.check_invariants().empty());
}

TEST_CASE("sailing: a stop mid-voyage trims the segment and the ship idles near where it was") {
    sim::World w = make_world(Spec{.ships = 1});
    REQUIRE(w.apply(move(0, 1, 90, 64)).ok());
    for (int i = 0; i < 400; ++i) {
        w.advance(10);
    }
    const sim::FixedPos mid = ship(w, 1).pos;
    CHECK(mid.x > tile_c(20) + 10 * kTile); // it sailed, and is nowhere near the goal
    CHECK(mid.x < tile_c(90) - 20 * kTile);
    REQUIRE(w.apply(stop(0, 1)).ok());
    CHECK_FALSE(ship(w, 1).nav->order.has_value());
    for (int i = 0; i < 200; ++i) {
        w.advance(10);
    }
    const sim::FixedPos rest = ship(w, 1).pos;
    // It sails out the remainder of its chunk (less than one tile) and stays.
    CHECK(rest.x >= mid.x);
    CHECK(rest.x - mid.x <= kTile);
    CHECK(rest.y == mid.y);
    for (int i = 0; i < 100; ++i) {
        w.advance(10);
    }
    CHECK(ship(w, 1).pos.x == rest.x);
    CHECK(w.check_invariants().empty());
}

TEST_CASE(
    "sailing: a ship that enters a blocked state drops its order and ticket with RouteFailed") {
    sim::World w = make_world(Spec{.ships = 1});
    REQUIRE(w.apply(move(0, 1, 90, 64)).ok());
    (void)w.drain_events();
    ship(w, 1).state_word |= 0x4; // in the blocked mask 0x200e
    w.advance(10);
    const auto failed = events_of(w, sim::SimEventKind::ShipRouteFailed);
    REQUIRE(failed.size() == 1);
    CHECK(failed[0].order_id == 1);
    CHECK(failed[0].code == static_cast<std::uint32_t>(sim::NavFailReason::ShipBlocked));
    CHECK_FALSE(ship(w, 1).nav->order.has_value());
    CHECK_FALSE(ship(w, 1).nav->ticket.has_value());
    CHECK(w.check_invariants().empty());
}

TEST_CASE("admission: sustained replacements cannot starve the youngest waiter, the quota holds") {
    // 100 registry slots (5 ships, 95 fillers), 5-tick Advances: ten slots are visited per Advance
    // (k = floor(5 * 100 / 50)), the quota is four plans per Advance. Ships 1..4 are re-commanded
    // on every Advance (the adversary of 12.2); ship 5 asked once, last, so it holds the youngest
    // ticket. The first-come rule starves it for ever; the fair rule serves it within
    // (p + 1) * C + R Advances = 5 * 10 + 43.
    sim::World w = make_world(Spec{.ships = 5, .scouts = 95, .players = 1});
    for (int s = 1; s <= 4; ++s) {
        REQUIRE(w.apply(move(0, static_cast<std::uint64_t>(s), 200 + s, 100 + 3 * s)).ok());
    }
    REQUIRE(w.apply(move(0, 5, 220, 130)).ok());
    CHECK(ship(w, 5).nav->ticket == std::optional<std::uint64_t>(5)); // the youngest
    int served_at = -1;
    std::uint64_t routed_total = 0;
    int max_per_advance = 0;
    for (int adv = 1; adv <= 400; ++adv) {
        std::vector<bool> routed_before;
        for (std::uint64_t id = 1; id <= 5; ++id) {
            const sim::NavMotion& m = *ship(w, id).nav;
            routed_before.push_back(m.order.has_value() && m.order->routed);
        }
        w.advance(10);
        int new_routed = 0;
        for (std::uint64_t id = 1; id <= 5; ++id) {
            const sim::NavMotion& m = *ship(w, id).nav;
            const bool now = m.order.has_value() && m.order->routed;
            if (now && !routed_before[id - 1]) {
                ++new_routed;
                if (id == 5 && served_at < 0) {
                    served_at = adv;
                }
            }
        }
        max_per_advance = std::max(max_per_advance, new_routed);
        routed_total += static_cast<std::uint64_t>(new_routed);
        // The adversary: replace the orders of ships 1..4 on every Advance.
        for (int s = 1; s <= 4; ++s) {
            REQUIRE(w.apply(move(0, static_cast<std::uint64_t>(s), 200 + s + (adv % 2) * 5,
                                 100 + 3 * s))
                        .ok());
        }
        REQUIRE(w.check_invariants().empty());
    }
    CHECK(served_at > 0);
    CHECK(served_at <= 5 * 10 + 43);
    CHECK(max_per_advance <= 4);
    CHECK(routed_total >= 5);
}

TEST_CASE("mid-voyage save and load, replay, and thread counts give the same voyage") {
    const auto run = [](const sim::Executor& ex, bool save_at_half, std::uint64_t& hash_out,
                        std::vector<std::uint8_t>& bytes_out) {
        sim::World w = make_world(Spec{.ships = 2, .scouts = 3}, ex);
        REQUIRE(w.apply(move(0, 1, 60, 80)).ok());
        REQUIRE(w.apply(move(0, 2, 40, 120)).ok());
        for (int i = 0; i < 700; ++i) {
            w.advance(10);
            if (save_at_half && i == 349) {
                auto loaded = sim::World::load(w.save(), ship_test::nav_ship_registry());
                REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
                REQUIRE(loaded.value().state_hash() == w.state_hash());
                w = std::move(loaded).value(); // continue from the reloaded world
            }
        }
        hash_out = w.state_hash();
        bytes_out = w.save();
        CHECK(w.check_invariants().empty());
    };
    std::uint64_t h_plain = 0, h_reload = 0, h_t2 = 0, h_t4 = 0, h_t7 = 0;
    std::vector<std::uint8_t> b_plain, b_reload, b_t2, b_t4, b_t7;
    run(sim::serial_executor(), false, h_plain, b_plain);
    run(sim::serial_executor(), true, h_reload, b_reload);
    island_test::ThreadExecutor t2(2), t4(4), t7(7);
    run(t2, false, h_t2, b_t2);
    run(t4, false, h_t4, b_t4);
    run(t7, false, h_t7, b_t7);
    CHECK(h_reload == h_plain);
    CHECK(b_reload == b_plain);
    CHECK(h_t2 == h_plain);
    CHECK(h_t4 == h_plain);
    CHECK(h_t7 == h_plain);
    CHECK(b_t7 == b_plain);
    // The voyage really happened (ship 1 left its start).
    sim::World w = make_world(Spec{.ships = 2, .scouts = 3});
    const sim::FixedPos start = ship(w, 1).pos;
    auto loaded = sim::World::load(b_plain, ship_test::nav_ship_registry());
    REQUIRE(loaded.ok());
    CHECK(ship(loaded.value(), 1).pos.x > start.x + 3 * kTile);
}

TEST_CASE(
    "replay: a recorded sailing session reproduces in a fresh world, another target differs") {
    const auto session = [](int target_y, sim::ReplayLog& log_out) {
        sim::World w = make_world(Spec{.ships = 1});
        auto started = sim::Recorder::start(w);
        REQUIRE(started.ok());
        sim::Recorder rec = std::move(started).value();
        REQUIRE(rec.submit(move(0, 1, 70, target_y)).ok());
        for (int block = 0; block < 4; ++block) {
            for (int i = 0; i < 100; ++i) {
                rec.advance(10);
            }
            rec.checkpoint();
        }
        REQUIRE(rec.submit(stop(0, 1)).ok());
        rec.advance(10);
        rec.checkpoint();
        log_out = rec.log();
        return w.state_hash();
    };
    sim::ReplayLog a, b;
    const std::uint64_t ha = session(64, a);
    const std::uint64_t hb = session(80, b);
    CHECK(ha != hb); // the target is a discriminating input of the whole voyage
    sim::World fresh = make_world(Spec{.ships = 1});
    auto outcome = sim::replay(fresh, a);
    REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
    CHECK(fresh.state_hash() == ha);
    // The log written to bytes and read back replays the same.
    auto bytes = sim::write_log(a);
    REQUIRE(bytes.ok());
    auto read = sim::read_log(bytes.value());
    REQUIRE(read.ok());
    sim::World fresh2 = make_world(Spec{.ships = 1});
    REQUIRE(sim::replay(fresh2, read.value()).ok());
    CHECK(fresh2.state_hash() == ha);
    // The other session's log refuses to replay against... itself fine, against the first world's
    // checkpoints it diverges: swap the logs' commands by replaying b on a fresh world and
    // comparing the final hash with a's.
    sim::World fresh3 = make_world(Spec{.ships = 1});
    REQUIRE(sim::replay(fresh3, b).ok());
    CHECK(fresh3.state_hash() == hb);
    CHECK(fresh3.state_hash() != ha);
}

namespace {

/// The admission adversary of 12.2: ships 1..4 are "greedy" (re-commanded on every Advance to stay
/// where they are: the route answers Arrived at once, so they are ready at every visit and consume
/// a quota unit each time), ship 5 is the victim (ordered once, or re-asked on every Advance).
/// Returns the Advance at which the victim's order was routed (0 = never within `limit`) and the
/// most plans any Advance started.
struct Starvation {
    int served_at = 0;
    int max_plans_per_advance = 0;
    int plans_total = 0;
};

Starvation starve(const Spec& spec, bool victim_reasks, int limit) {
    sim::World w = make_world(spec);
    const auto issuer = [&](std::uint64_t id) { return id <= 4 ? spec.first_four_owner : 0U; };
    const auto stay = [&](std::uint64_t id) {
        const sim::FixedPos p = ship(w, id).pos;
        return sim::Command{sim::PlayerId{issuer(id)},
                            sim::MoveCmd{sim::FigureId{id}, (p.x / kTile) * kTile + 128,
                                         (p.y / kTile) * kTile + 128}};
    };
    for (std::uint64_t id = 1; id <= 4; ++id) {
        REQUIRE(w.apply(stay(id)).ok());
    }
    REQUIRE(w.apply(move(0, 5, 220, 130)).ok()); // the youngest ticket
    Starvation out;
    for (int adv = 1; adv <= limit; ++adv) {
        w.advance(10);
        int plans = 0;
        for (const sim::SimEvent& e : w.drain_events()) {
            if (e.kind == sim::SimEventKind::ShipArrived) {
                ++plans; // a greedy ship's plan (Arrived)
            }
        }
        const sim::NavMotion& v = *ship(w, 5).nav;
        if (out.served_at == 0 && v.order.has_value() && v.order->routed) {
            out.served_at = adv;
            ++plans;
        }
        out.max_plans_per_advance = std::max(out.max_plans_per_advance, plans);
        out.plans_total += plans;
        for (std::uint64_t id = 1; id <= 4; ++id) {
            REQUIRE(w.apply(stay(id)).ok());
        }
        if (victim_reasks && out.served_at == 0) {
            REQUIRE(w.apply(move(0, 5, 220 + adv % 3, 130)).ok());
        }
        REQUIRE(w.check_invariants().empty());
    }
    return out;
}

} // namespace

TEST_CASE(
    "admission: greedy ready ships cannot starve the oldest waiter (the first-come rule does)") {
    // 100 slots, ten visited per Advance, quota four: whenever the window holds the four greedy
    // ships and the victim, a first-come rule serves the greedy ones first and, because they are
    // ready at every visit and re-commanded on every Advance, never reaches the victim. The fair
    // rule keeps one unit for the older ticket: the fourth greedy ship is deferred and the victim
    // planned. Bound: (p + 1) * C + R = 1 * 10 + 43 Advances for the oldest ticket (p = 0).
    const Spec spec{.ships = 5, .scouts = 95, .players = 2};
    const Starvation plain = starve(spec, false, 150);
    CHECK(plain.served_at > 0);
    CHECK(plain.served_at <= 1 * 10 + 43);
    // The full quota is used (a rule that wastes a unit, e.g. counting its own ticket, serves
    // fewer).
    CHECK(plain.max_plans_per_advance == 4);
    // The victim re-asked on every Advance keeps its ticket and so its place: also served.
    const Starvation reasked = starve(spec, true, 150);
    CHECK(reasked.served_at > 0);
    CHECK(reasked.served_at <= 1 * 10 + 43);
    // Tickets of another player's ships count too (the rule is global, not per owner).
    Spec other = spec;
    other.first_four_owner = 1;
    const Starvation foreign = starve(other, false, 150);
    CHECK(foreign.served_at > 0);
    CHECK(foreign.served_at <= 1 * 10 + 43);
    CHECK(foreign.max_plans_per_advance == 4);
}

TEST_CASE("admission: removing a waiting ship, and saving and loading while tickets wait") {
    sim::World w = make_world(Spec{.ships = 5, .scouts = 95, .players = 1});
    for (std::uint64_t id = 1; id <= 5; ++id) {
        REQUIRE(w.apply(move(0, id, 200 + static_cast<int>(id), 100)).ok());
    }
    // Save and load with five tickets pending: the continuation is identical.
    auto loaded = sim::World::load(w.save(), ship_test::nav_ship_registry());
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    sim::World copy = std::move(loaded).value();
    CHECK(copy.state_hash() == w.state_hash());
    for (int i = 0; i < 30; ++i) {
        w.advance(10);
        copy.advance(10);
        REQUIRE(copy.state_hash() == w.state_hash());
    }
    // A ship removed while it waits takes its ticket with it; the others are still served.
    sim::World v = make_world(Spec{.ships = 5, .scouts = 95, .players = 1});
    for (std::uint64_t id = 1; id <= 5; ++id) {
        REQUIRE(v.apply(move(0, id, 200 + static_cast<int>(id), 100)).ok());
    }
    REQUIRE(sim::remove_figure(sim::WorldTestAccess::figures(v), sim::FigureId{1}).has_value());
    CHECK(v.check_invariants().empty());
    bool all_routed = false;
    for (int i = 0; i < 120 && !all_routed; ++i) {
        v.advance(10);
        all_routed = true;
        for (std::uint64_t id = 2; id <= 5; ++id) {
            all_routed =
                all_routed && ship(v, id).nav->order.has_value() && ship(v, id).nav->order->routed;
        }
    }
    CHECK(all_routed);
    CHECK(v.check_invariants().empty());
}

namespace {

/// An open-sea world with one flat island of land (heights 30, far above the water level 19) at
/// (200, 100) of 64 x 64 tiles, ship 1 at (20, 64) of player 0.
sim::World island_world(const sim::Rules& rules) {
    sim::ScenarioSpec spec = spec_of(Spec{.ships = 1, .players = 1});
    spec.islands.push_back(syn::plain_island(0, 200, 100, 64, 64));
    auto created = sim::World::create(spec, ship_test::nav_ship_registry(), {}, rules, {},
                                      sim::serial_executor());
    REQUIRE_MESSAGE(created.ok(), (created.ok() ? std::string() : created.error()));
    sim::World w = std::move(created).value();
    sim::Island& is = *sim::WorldTestAccess::islands(w)[0];
    is.terrain = sim::make_terrain(std::vector<std::uint8_t>(std::size_t{64} * 64, 30), {}, {});
    sim::WorldTestAccess::route_cache(w).clear();
    return w;
}

sim::SimEvent failure_of(sim::World& w, int limit) {
    for (int i = 0; i < limit; ++i) {
        w.advance(10);
        for (const sim::SimEvent& e : w.drain_events()) {
            CHECK(e.kind != sim::SimEventKind::ShipArrived);
            if (e.kind == sim::SimEventKind::ShipRouteFailed) {
                return e;
            }
        }
    }
    FAIL("no RouteFailed event");
    return {};
}

} // namespace

TEST_CASE("route failures: unreachable, budget exceeded and outside the window are later events") {
    // Unreachable: the goal is in the middle of the island, more than 6 tiles from any water; the
    // order is accepted (the target is inside the world) and the plan fails.
    {
        sim::World w = island_world({});
        REQUIRE(w.apply(move(0, 1, 232, 132)).ok());
        const sim::FixedPos start = ship(w, 1).pos;
        const sim::SimEvent e = failure_of(w, 200);
        CHECK(e.order_id == 1);
        CHECK(e.code == static_cast<std::uint32_t>(sim::NavFailReason::Unreachable));
        CHECK_FALSE(ship(w, 1).nav->order.has_value());
        CHECK_FALSE(ship(w, 1).nav->ticket.has_value());
        CHECK(ship(w, 1).pos.x == start.x);
        CHECK(ship(w, 1).pos.y == start.y);
        CHECK(w.check_invariants().empty());
        // The control: a goal in the sea of the same world is reached.
        sim::World ok = island_world({});
        REQUIRE(ok.apply(move(0, 1, 60, 70)).ok());
        bool arrived = false;
        for (int i = 0; i < 1800 && !arrived; ++i) {
            ok.advance(10);
            for (const sim::SimEvent& ev : ok.drain_events()) {
                arrived = arrived || ev.kind == sim::SimEventKind::ShipArrived;
            }
        }
        CHECK(arrived);
    }
    // BudgetExceeded: a coarse budget of one pop cannot plan a 50-tile route; the same world with
    // the default budget plans it.
    {
        sim::Rules tight;
        tight.nav.coarse_budget = 1;
        sim::World w = island_world(tight);
        REQUIRE(w.apply(move(0, 1, 70, 64)).ok());
        const sim::SimEvent e = failure_of(w, 100);
        CHECK(e.code == static_cast<std::uint32_t>(sim::NavFailReason::BudgetExceeded));
        CHECK_FALSE(ship(w, 1).nav->order.has_value());
        sim::World ok = island_world({});
        REQUIRE(ok.apply(move(0, 1, 70, 64)).ok());
        ok.advance(100);
        CHECK(ok.drain_events().size() == 0);
        CHECK(ok.check_invariants().empty());
    }
    // OutsideWindow: a window of one tile around the start cannot hold the goal disc.
    {
        sim::Rules small;
        small.nav.window_radius_tiles = 1;
        sim::World w = island_world(small);
        REQUIRE(w.apply(move(0, 1, 70, 64)).ok());
        const sim::SimEvent e = failure_of(w, 100);
        CHECK(e.code == static_cast<std::uint32_t>(sim::NavFailReason::OutsideWindow));
    }
}

TEST_CASE(
    "plan nodes: the C++ conversion equals the plannodes vectors of the independent reference") {
    std::ifstream in(std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "nav" /
                     "movement_vectors.txt");
    REQUIRE(in.good());
    const sim::NavFacts facts{154618816U, 472446400U, 6000U, 4U, 2U, 0U, 1280U};
    const std::vector<std::string> names = {"W", "SW", "S", "SE", "E", "NE", "N", "NW"};
    std::string line;
    int cases = 0;
    while (std::getline(in, line)) {
        if (line.rfind("plannodes ", 0) != 0) {
            continue;
        }
        std::istringstream ls(line.substr(10));
        unsigned facing = 0;
        ls >> facing;
        std::vector<sim::RouteRun> runs;
        std::string tok;
        while (ls >> tok && tok != "|") {
            std::size_t k = 0;
            while (k < tok.size() && std::isalpha(static_cast<unsigned char>(tok[k]))) {
                ++k;
            }
            const auto it = std::find(names.begin(), names.end(), tok.substr(0, k));
            REQUIRE(it != names.end());
            runs.push_back(sim::RouteRun{static_cast<std::uint8_t>(it - names.begin()),
                                         static_cast<std::uint8_t>(std::stoi(tok.substr(k)))});
        }
        std::vector<std::string> want;
        while (ls >> tok) {
            want.push_back(tok);
        }
        const std::vector<sim::NavNode> got = sim::nav_nodes_of_runs(runs, facing, facts);
        REQUIRE(got.size() == want.size());
        for (std::size_t i = 0; i < got.size(); ++i) {
            INFO(line << " node " << i);
            if (want[i][0] == 'T') {
                CHECK(got[i].kind == sim::NavSegKind::Turn);
                CHECK(got[i].facing == std::stoi(want[i].substr(1)));
                CHECK(got[i].length_q32 == facts.turn_step_q32);
            } else {
                const int comma = static_cast<int>(want[i].find(','));
                const int dir = std::stoi(want[i].substr(1, static_cast<std::size_t>(comma) - 1));
                const std::uint64_t steps =
                    std::stoull(want[i].substr(static_cast<std::size_t>(comma) + 1));
                CHECK(got[i].kind == sim::NavSegKind::Move);
                CHECK(got[i].dir8 == dir);
                CHECK(got[i].length_q32 ==
                      steps * ((dir & 1) != 0 ? sim::kNavDiagStepQ32 : sim::kNavUnitQ32));
                CHECK(got[i].speed_q32 == facts.speed_q32);
            }
        }
        ++cases;
    }
    CHECK(cases >= 7);
}

TEST_CASE(
    "route cache: it ignores placed objects (a continuous world and a reloaded one plan alike)") {
    sim::World w = island_world({});
    sim::RouteCache& cache = sim::WorldTestAccess::route_cache(w);
    const sim::CoastIndex& cached = cache.get(w);
    CHECK(cache.built());
    // An object appears on an island tile without any command (island growth does this).
    sim::Island& is = *sim::WorldTestAccess::islands(w)[0];
    const sim::Slot idx = is.tiles.index_of(210, 110);
    REQUIRE(idx != sim::kNoSlot);
    is.tiles.set_object(idx, 1201);
    const sim::WorldTileSource fresh_src(w, sim::ship_route_terrain_options());
    CHECK(cached.same_words(sim::CoastIndex(fresh_src, {})));
    // Copying the world drops the derived cache; a moved-from cache is empty too.
    sim::World copy = w;
    CHECK_FALSE(sim::WorldTestAccess::route_cache(copy).built());
    sim::World moved = std::move(w);
    CHECK_FALSE(sim::WorldTestAccess::route_cache(moved).built());
}
