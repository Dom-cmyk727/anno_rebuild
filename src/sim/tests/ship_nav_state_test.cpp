// The navigation facts and the persisted ship motion state (task MAP-NAV-2 package B; contract
// docs/sim/ship-navigation.md 12.4, 12.5). Synthetic: no game files. Expected values are derived
// by hand or by tools/figure_digest_ref.py / Python big integers, never read back from the code.
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/figuren.hpp>
#include <anno/sim/archive.hpp>
#include <anno/sim/ship.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim/world.hpp>

#include "island_helpers.hpp"
#include "ship_helpers.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
namespace fmt = anno::formats;
namespace syn = anno::sim::synthetic;
using ship_test::kDonkey;
using ship_test::kScout;
using ship_test::kShip;

namespace {

/// The facts of the shipped type 1502 (HEAD f0 0.11, f1 0.036; LOGIC holds 4/2, class 0, 6000 hp,
/// stamp byte 40), converted by hand: f32(0.036) = 0.035999998450279236 * 2^32 = 154618816 exactly,
/// f32(0.11) = 0.10999999940395355 * 2^32 = 472446400 (Python Fraction, round half even).
constexpr sim::NavFacts kFacts{154618816U, 472446400U, 6000U, 4U, 2U, 0U, 1280U};

fmt::chunk::Chunk record(const char* name, const std::vector<std::uint8_t>& payload) {
    fmt::chunk::Chunk c;
    std::copy_n(name, std::strlen(name), c.name_field.begin());
    c.payload = payload;
    return c;
}

std::vector<std::uint8_t> head_of(float f0, float f1, std::size_t len = 20) {
    std::vector<std::uint8_t> head(len, 0);
    const std::uint32_t b0 = std::bit_cast<std::uint32_t>(f0),
                        b1 = std::bit_cast<std::uint32_t>(f1);
    for (int i = 0; i < 4 && static_cast<std::size_t>(i + 4) < len; ++i) {
        head[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(b0 >> (8 * i));
        head[static_cast<std::size_t>(i + 4)] = static_cast<std::uint8_t>(b1 >> (8 * i));
    }
    return head;
}

std::vector<std::uint8_t> logic_of(std::uint32_t hp, std::uint8_t stamp = 40,
                                   std::uint8_t cls = 0) {
    std::vector<std::uint8_t> logic(44, 0);
    logic[0x10] = cls;
    logic[0x13] = 4;
    logic[0x14] = 2;
    logic[0x1e] = static_cast<std::uint8_t>(hp);
    logic[0x1f] = static_cast<std::uint8_t>(hp >> 8);
    logic[0x25] = 128;
    logic[0x28] = stamp;
    return logic;
}

std::optional<sim::NavFacts> loaded(const char* kind, const std::vector<std::uint8_t>& head,
                                    const std::vector<std::uint8_t>& logic) {
    std::vector<std::uint8_t> name(kind, kind + std::strlen(kind) + 1);
    fmt::figuren::Entry e;
    e.id = 1502;
    e.records = {record("HEAD", head), record("BEWOHNTYPE", name), record("LOGIC", logic)};
    auto built = sim::TypeRegistry::build({}, sim::original_goods(), {}, std::span(&e, 1));
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    return built.value()->find_figure_type(1502)->nav;
}

std::string load_error(const char* kind, const std::vector<std::uint8_t>& head,
                       const std::vector<std::uint8_t>& logic) {
    std::vector<std::uint8_t> name(kind, kind + std::strlen(kind) + 1);
    fmt::figuren::Entry e;
    e.id = 1502;
    e.records = {record("HEAD", head), record("BEWOHNTYPE", name), record("LOGIC", logic)};
    auto built = sim::TypeRegistry::build({}, sim::original_goods(), {}, std::span(&e, 1));
    return built.ok() ? std::string() : built.error();
}

anno::formats::Result<std::shared_ptr<const sim::TypeRegistry>, std::string>
seal_nav(const sim::NavFacts& n) {
    sim::TypeRegistry::Builder b;
    sim::FigureType t;
    t.id = 1502;
    t.bewohntype = 113;
    t.range_88 = 16 * 256;
    t.nav = n;
    b.figure_type(t);
    return b.build();
}

/// The expedition registry with a navigating ship type 1502 (scout and donkey do not navigate).
std::shared_ptr<const sim::TypeRegistry> nav_registry(std::uint32_t nav_id = kShip,
                                                      const sim::NavFacts& facts = kFacts) {
    auto base = syn::make_synthetic_types();
    REQUIRE(base);
    sim::TypeRegistry::Builder b;
    b.walk_tables(sim::synthetic_walk_tables());
    for (const sim::BuildingType& t : base.value().registry->types()) {
        b.add(t);
    }
    sim::FigureType ship{nav_id, 0, 0, 0, 0};
    ship.bewohntype = 113;
    ship.nav = facts;
    if (nav_id != kShip) {
        // 1502 stays a ship-class type without navigation facts: nothing special about the id.
        sim::FigureType plain{kShip, 0, 0, 0, 0};
        plain.bewohntype = 113;
        b.figure_type(plain);
    }
    b.figure_type(ship)
        .figure_type(sim::FigureType{kScout, 0, 0, 0, 0})
        .figure_type(sim::FigureType{kDonkey, 0, 0, 0, 0});
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    return built.value();
}

/// Two navigating ships (ids 1, 2) and a scout (id 3, not navigating).
sim::World nav_world(const std::shared_ptr<const sim::TypeRegistry>& types) {
    sim::ScenarioSpec spec = syn::empty_scenario(77);
    for (std::uint32_t p = 0; p < 2; ++p) {
        sim::ScenarioSpec::PlayerSpec ps;
        ps.id = sim::PlayerId{p};
        ps.money = 1000;
        spec.players.push_back(ps);
    }
    for (std::uint32_t i = 0; i < 3; ++i) {
        sim::ScenarioSpec::FigureSpec f;
        f.kind = i < 2 ? kShip : kScout;
        f.owner = sim::PlayerId{i < 2 ? i : 0};
        f.pos =
            sim::FixedPos{static_cast<std::int32_t>(256 * (10 + 20 * i) + 128), 256 * 100 + 128};
        f.dir16 = 6;
        spec.figures.push_back(f);
    }
    auto w = sim::World::create(spec, types);
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    return std::move(w).value();
}

sim::Figure& figure(sim::World& w, std::uint64_t id) {
    sim::Figure* f = sim::find_figure(sim::WorldTestAccess::figures(w), sim::FigureId{id});
    REQUIRE(f != nullptr);
    return *f;
}

bool has(const std::string& text, const char* needle) {
    return text.find(needle) != std::string::npos;
}

/// A busy but valid motion: a diagonal movement with a residue, an order, a ticket, pending nodes.
void make_busy(sim::World& w, std::uint64_t id) {
    sim::IdCounters& ids = sim::WorldTestAccess::ids(w);
    ids.next_order_id = 100;
    ids.next_ticket = 50;
    sim::Figure& f = figure(w, id);
    f.dir16 = 6; // dir8 3 (SE) x 2
    sim::NavMotion& m = *f.nav;
    m.order = sim::NavOrder{7, true, 12345, -678};
    m.ticket.reset();
    m.seg = sim::NavSegment{sim::NavSegKind::Move, 3, 87, kFacts.speed_q32, std::uint64_t{1} << 31};
    m.nodes = {
        sim::NavNode{sim::NavSegKind::Turn, 0, 4, kFacts.turn_step_q32, sim::kNavIdleSpeedQ32},
        sim::NavNode{sim::NavSegKind::Move, 4, 0, 3 * sim::kNavUnitQ32, kFacts.speed_q32}};
    m.last = 0xFFF0;
    m.res24[0] = 12345;
    m.res24[1] = 0xFFFFFF;
    m.pos_u[0] = std::int64_t{f.pos.x} * sim::kNavPosUnitsPerQ8 + 777;
    m.pos_u[1] = std::int64_t{f.pos.y} * sim::kNavPosUnitsPerQ8 + (sim::kNavPosUnitsPerQ8 - 1);
}

/// An accepted order waiting for its planning slot: unrouted, holding ticket `ticket`, no plan.
void make_waiting(sim::World& w, std::uint64_t id, std::uint64_t order, std::uint64_t ticket) {
    sim::IdCounters& ids = sim::WorldTestAccess::ids(w);
    ids.next_order_id = 100;
    ids.next_ticket = 50;
    sim::NavMotion& m = *figure(w, id).nav;
    m.order = sim::NavOrder{order, false, 300, 400};
    m.ticket = ticket;
    // The registry's derived ticket index cannot see this in-place edit.
    sim::rebuild_figure_index(sim::WorldTestAccess::figures(w));
}

} // namespace

TEST_CASE("nav facts: the loader converts the file's f32 values and bytes exactly") {
    const auto n = loaded("SHIP", head_of(0.11F, 0.036F), logic_of(6000));
    REQUIRE(n.has_value());
    CHECK(*n == kFacts);
    // The stamp byte 0 means 2 tiles (512/256), any other value is byte / 8 tile = 32 per byte.
    CHECK(loaded("SHIP", head_of(0.11F, 0.036F), logic_of(6000, 0))->stamp_radius_88 == 512U);
    CHECK(loaded("SHIP", head_of(0.11F, 0.036F), logic_of(6000, 255))->stamp_radius_88 == 8160U);
    CHECK(loaded("SHIP", head_of(0.11F, 0.036F), logic_of(6000, 1))->stamp_radius_88 == 32U);
    CHECK(loaded("SHIP", head_of(0.11F, 0.036F), logic_of(6000, 40, 3))->nav_class == 3U);
    CHECK(loaded("SHIP", head_of(0.11F, 0.036F), logic_of(65535))->max_hp == 65535U);
    // Rounding to nearest even: f32 values that are exact multiples of 2^-32 convert exactly.
    CHECK(loaded("SHIP", head_of(0.5F, 0.0625F * 0.5F), logic_of(1))->turn_step_q32 == (1U << 31));
    CHECK(loaded("SHIP", head_of(0.5F, 0.0625F * 0.5F), logic_of(1))->speed_q32 == (1U << 27));
    // Rounding to nearest even below 2^-9 (the shipped values convert exactly): expected values
    // are Python Fractions of f32 * 2^32. 3*2^-33 -> 1.5 (tie) -> 2; 5*2^-33 -> 2.5 (tie) -> 2;
    // 7*2^-33 -> 3.5 (tie) -> 4; 5*2^-34 -> 1.25 -> 1; 7*2^-34 -> 1.75 -> 2.
    const auto speed_of = [](float f1) {
        return loaded("SHIP", head_of(0.5F, f1), logic_of(10))->speed_q32;
    };
    CHECK(speed_of(std::ldexp(3.0F, -33)) == 2U);
    CHECK(speed_of(std::ldexp(5.0F, -33)) == 2U);
    CHECK(speed_of(std::ldexp(7.0F, -33)) == 4U);
    CHECK(speed_of(std::ldexp(5.0F, -34)) == 1U);
    CHECK(speed_of(std::ldexp(7.0F, -34)) == 2U);
    CHECK(speed_of(std::ldexp(1.0F, -31)) == 2U); // exact
    CHECK(loaded("SHIP", head_of(std::ldexp(7.0F, -34), 0.036F), logic_of(10))->turn_step_q32 ==
          2U);
    // Just below the speed domain is kept; the edge itself, and beyond, is a load error naming the
    // field (a mod ship that silently could not be ordered would be worse).
    const float below = std::nextafter(0.0625F, 0.0F); // 2^-4 - 2^-28 -> q32 2^28 - 16
    REQUIRE(loaded("SHIP", head_of(0.11F, below), logic_of(10)).has_value());
    CHECK(loaded("SHIP", head_of(0.11F, below), logic_of(10))->speed_q32 == (1U << 28) - 16U);
    const auto navigation_error = [](float f0, float f1, const std::vector<std::uint8_t>& logic) {
        return load_error("SHIP", head_of(f0, f1), logic);
    };
    CHECK(navigation_error(0.11F, 0.0625F, logic_of(10)) ==
          "figure 1502: navigation: speed_q32 268435456 is not in 1 .. 268435455");
    CHECK(navigation_error(0.11F, 0.5F, logic_of(10)) ==
          "figure 1502: navigation: speed_q32 2147483648 is not in 1 .. 268435455");
    CHECK(navigation_error(0.11F, 0.0F, logic_of(10)) ==
          "figure 1502: navigation: speed_q32 0 is not in 1 .. 268435455");
    CHECK(navigation_error(0.11F, std::ldexp(1.0F, -33), logic_of(10)) ==
          "figure 1502: navigation: speed_q32 0 is not in 1 .. 268435455"); // tie 0.5 -> even 0
    CHECK(navigation_error(0.11F, std::numeric_limits<float>::denorm_min(), logic_of(10)) ==
          "figure 1502: navigation: speed_q32 0 is not in 1 .. 268435455");
    CHECK(navigation_error(0.0F, 0.036F, logic_of(10)) ==
          "figure 1502: navigation: turn_step_q32 is 0");
    CHECK(navigation_error(1.0F, 0.036F, logic_of(10)) ==
          "figure 1502: navigation: HEAD speed or turn step is not a representable length");
    {
        // Both hold counts 0 (the shipped type 391 has none): the type navigates; the cargo term
        // of the speed factor is 0 for it (ship-navigation.md 12.4, no capacity, no cargo).
        std::vector<std::uint8_t> no_holds = logic_of(10);
        no_holds[0x13] = no_holds[0x14] = 0;
        const auto nh = loaded("SHIP", head_of(0.11F, 0.036F), no_holds);
        REQUIRE(nh.has_value());
        CHECK(nh->hold_count_a == 0U);
        CHECK(nh->hold_count_b == 0U);
    }
    // A type without hit points (the shipped 343 and 1510) is simply not a navigating type,
    // whatever else its data says.
    CHECK_FALSE(loaded("SHIP", head_of(0.11F, 0.036F), logic_of(0)).has_value());
    CHECK_FALSE(loaded("SHIP", head_of(0.0F, 0.5F), logic_of(0)).has_value());
    // A negative or NaN speed, or a HEAD shorter than 8 bytes, is refused by the existing loader
    // checks before any nav fact is derived.
    CHECK(load_error("SHIP", head_of(0.11F, -0.036F), logic_of(10)) ==
          "figure 1502: HEAD speed is not a speed");
    CHECK(load_error("SHIP", head_of(0.11F, std::numeric_limits<float>::quiet_NaN()),
                     logic_of(10)) == "figure 1502: HEAD speed is not a speed");
    CHECK(load_error("SHIP", head_of(0.11F, 0.036F, 4), logic_of(6000)) ==
          "figure 1502: no HEAD record of 8 bytes");
    // A HEAD or LOGIC too short, or another class with the same bytes: absent.
    CHECK_FALSE(loaded("ERKUNDER", head_of(0.11F, 0.036F), logic_of(6000)).has_value());
    CHECK_FALSE(loaded("ESEL", head_of(0.11F, 0.036F), logic_of(6000)).has_value());
}

TEST_CASE("nav facts: the registry seal refuses every value outside its domain") {
    // The valid edges.
    for (const sim::NavFacts ok :
         {sim::NavFacts{1, 1, 1, 0, 0, 0, 1},
          sim::NavFacts{(1U << 28) - 1, 0xFFFFFFFFU, 65535, 255, 255, 255, 8160}, kFacts}) {
        const auto r = seal_nav(ok);
        REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
        CHECK(*r.value()->find_figure_type(1502)->nav == ok);
    }
    struct Bad {
        const char* what;
        sim::NavFacts n;
    };
    sim::NavFacts f = kFacts;
    const auto with = [&](auto edit) {
        sim::NavFacts n = kFacts;
        edit(n);
        return n;
    };
    const Bad bad[] = {
        {"speed_q32 0", with([](auto& n) { n.speed_q32 = 0; })},
        {"speed_q32 2^28", with([](auto& n) { n.speed_q32 = 1U << 28; })},
        {"speed_q32 max", with([](auto& n) { n.speed_q32 = 0xFFFFFFFFU; })},
        {"turn_step_q32 0", with([](auto& n) { n.turn_step_q32 = 0; })},
        {"max_hp 0", with([](auto& n) { n.max_hp = 0; })},
        {"max_hp 65536", with([](auto& n) { n.max_hp = 65536; })},
        {"hold_count_a 256", with([](auto& n) { n.hold_count_a = 256; })},
        {"hold_count_b 256", with([](auto& n) { n.hold_count_b = 256; })},
        {"nav_class 256", with([](auto& n) { n.nav_class = 256; })},
        {"stamp 0", with([](auto& n) { n.stamp_radius_88 = 0; })},
        {"stamp 8161", with([](auto& n) { n.stamp_radius_88 = 8161; })},
    };
    (void)f;
    for (const Bad& b : bad) {
        INFO(b.what);
        const auto r = seal_nav(b.n);
        REQUIRE_FALSE(r.ok());
        CHECK(has(r.error(), "figure type 1502 navigation fact:"));
    }
}

TEST_CASE("nav motion: a scenario gives idle motion to navigating ships only") {
    const auto types = nav_registry();
    sim::World w = nav_world(types);
    CHECK(w.check_invariants().empty());
    for (const std::uint64_t id : {1ULL, 2ULL}) {
        const sim::Figure& f = figure(w, id);
        REQUIRE(f.nav.has_value());
        CHECK(f.nav->seg.kind == sim::NavSegKind::Idle);
        CHECK(f.nav->seg.speed_q32 == 42949672U);
        CHECK(f.nav->seg.remaining_x500 == 42949672ULL * 500);
        CHECK(f.nav->seg.factor_percent == 100U);
        CHECK_FALSE(f.nav->order.has_value());
        CHECK_FALSE(f.nav->ticket.has_value());
        CHECK(f.nav->nodes.empty());
        CHECK(f.nav->pos_u[0] == std::int64_t{f.pos.x} * (std::int64_t{500} << 24));
        CHECK(f.nav->pos_u[1] == std::int64_t{f.pos.y} * (std::int64_t{500} << 24));
    }
    CHECK_FALSE(figure(w, 3).nav.has_value());
    CHECK(w.ids().next_order_id == 1);
    CHECK(w.ids().next_ticket == 1);
}

TEST_CASE("nav motion: invalid states are refused with a diagnostic, valid controls pass") {
    const auto types = nav_registry();
    const sim::World base = nav_world(types);
    // Ship 1: a routed diagonal movement with pending nodes. Ship 2: an unrouted order waiting.
    sim::World busy = base;
    make_busy(busy, 1);
    make_waiting(busy, 2, 8, 9);
    REQUIRE_MESSAGE(busy.check_invariants().empty(), busy.check_invariants());
    struct Case {
        const char* needle;
        void (*edit)(sim::World&);
    };
    const auto turn_ship2 = [](sim::World& w) {
        sim::NavSegment& g = figure(w, 2).nav->seg;
        g.kind = sim::NavSegKind::Turn;
        g.speed_q32 = sim::kNavIdleSpeedQ32;
        g.remaining_x500 = kFacts.turn_step_q32 * 500ULL;
    };
    (void)turn_ship2;
    const Case cases[] = {
        {"figures.list.slots[0].nav: missing for figure type",
         [](sim::World& w) { figure(w, 1).nav.reset(); }},
        {"figures.list.slots[2].nav: present for figure type",
         [](sim::World& w) { figure(w, 3).nav = sim::make_nav_motion(figure(w, 3).pos, 0); }},
        {"nav.seg.factor_percent: 0",
         [](sim::World& w) { figure(w, 1).nav->seg.factor_percent = 0; }},
        {"nav.seg.factor_percent: 101",
         [](sim::World& w) { figure(w, 1).nav->seg.factor_percent = 101; }},
        {"nav.seg.dir8: 8", [](sim::World& w) { figure(w, 1).nav->seg.dir8 = 8; }},
        {"needs the facing 2 * dir8", [](sim::World& w) { figure(w, 1).dir16 = 7; }},
        {"needs the facing 2 * dir8", [](sim::World& w) { figure(w, 1).dir16 = 4; }},
        {"a movement chunk is at most 1.0 + f32(1e-4)",
         [](sim::World& w) {
             figure(w, 1).nav->seg.remaining_x500 = sim::kNavChunkLimitQ32 * sim::kNavScale + 1;
         }},
        {"nav.seg.speed_q32: outside", [](sim::World& w) { figure(w, 1).nav->seg.speed_q32 = 0; }},
        {"nav.seg.speed_q32: outside",
         [](sim::World& w) { figure(w, 1).nav->seg.speed_q32 = 1U << 28; }},
        {"turn or idle segment has factor 100",
         [](sim::World& w) { figure(w, 2).nav->seg.factor_percent = 99; }},
        {"turn or idle segment has factor 100",
         [](sim::World& w) { figure(w, 2).nav->seg.dir8 = 1; }},
        {"idle segment runs at 0.01 with at most 1.0 left",
         [](sim::World& w) { figure(w, 2).nav->seg.remaining_x500 = (1ULL << 32) * 500 + 1; }},
        {"idle segment runs at 0.01 with at most 1.0 left",
         [](sim::World& w) { figure(w, 2).nav->seg.speed_q32 = 42949673U; }},
        {"a turn segment runs at 0.01",
         [](sim::World& w) {
             figure(w, 2).nav->seg = sim::NavSegment{sim::NavSegKind::Turn, 0, 100, 42949673U, 1};
         }},
        {"above the type's turn step",
         [](sim::World& w) {
             figure(w, 2).nav->seg = sim::NavSegment{sim::NavSegKind::TurnPhase, 0, 100, 42949672U,
                                                     472446400ULL * 500 + 1};
         }},
        {"slots[1].dir16: 16 is not in 0 .. 15", [](sim::World& w) { figure(w, 2).dir16 = 16; }},
        {"order_id: not below ids.next_order_id",
         [](sim::World& w) { figure(w, 1).nav->order->order_id = 0; }},
        {"order_id: not below ids.next_order_id",
         [](sim::World& w) { figure(w, 1).nav->order->order_id = 100; }},
        {"order_id: duplicated across ships",
         [](sim::World& w) { figure(w, 2).nav->order->order_id = 7; }},
        {"nav.ticket: not below ids.next_ticket",
         [](sim::World& w) { *figure(w, 2).nav->ticket = 50; }},
        {"nav.ticket: not below ids.next_ticket",
         [](sim::World& w) { *figure(w, 2).nav->ticket = 0; }},
        {"nav.ticket: duplicated across ships",
         [](sim::World& w) {
             // Ship 1 becomes a second waiting ship with the same ticket.
             sim::NavMotion& m = *figure(w, 1).nav;
             m.nodes.clear();
             m.seg = sim::make_nav_motion(figure(w, 1).pos, 0).seg;
             m.res24[0] = m.res24[1] = 0;
             figure(w, 1).dir16 = 6;
             m.order->routed = false;
             m.ticket = 9;
         }},
        {"a ticket or pending nodes without an order",
         [](sim::World& w) { figure(w, 2).nav->order.reset(); }},
        {"an unrouted order needs its ticket and has no nodes",
         [](sim::World& w) { figure(w, 2).nav->ticket.reset(); }},
        {"an unrouted order needs its ticket and has no nodes",
         [](sim::World& w) {
             figure(w, 2).nav->nodes.push_back(
                 sim::NavNode{sim::NavSegKind::Move, 4, 0, sim::kNavUnitQ32, kFacts.speed_q32});
         }},
        {"a routed order has consumed its ticket",
         [](sim::World& w) { figure(w, 1).nav->ticket = 3; }},
        {"res24: not below 2^24", [](sim::World& w) { figure(w, 1).nav->res24[0] = 1U << 24; }},
        {"res24: non-zero outside a diagonal movement segment",
         [](sim::World& w) {
             figure(w, 1).nav->seg.dir8 = 4;
             figure(w, 1).dir16 = 8;
         }},
        {"res24: non-zero outside a diagonal movement segment",
         [](sim::World& w) { figure(w, 1).nav->seg.remaining_x500 = 0; }},
        {"Figure::pos does not equal floor(pos_u) on axis 0",
         [](sim::World& w) { figure(w, 1).nav->pos_u[0] += sim::kNavPosUnitsPerQ8; }},
        {"Figure::pos does not equal floor(pos_u) on axis 1",
         [](sim::World& w) { figure(w, 1).nav->pos_u[1] -= 800LL << 24; }},
        {"nav.pos_u: outside +-2^61",
         [](sim::World& w) { figure(w, 1).nav->pos_u[0] = std::int64_t{1} << 61; }},
        {".pos: outside the world bound",
         [](sim::World& w) { figure(w, 1).pos.x = (1 << 22) + 1; }},
        {"nodes[0]: a turn node must carry facing < 16",
         [](sim::World& w) { figure(w, 1).nav->nodes[0].facing = 16; }},
        {"nodes[0]: a turn node must carry facing < 16",
         [](sim::World& w) { figure(w, 1).nav->nodes[0].length_q32 += 1; }},
        {"nodes[0]: a turn node must carry facing < 16",
         [](sim::World& w) { figure(w, 1).nav->nodes[0].speed_q32 = 1; }},
        {"nodes[0]: a turn node must carry facing < 16",
         [](sim::World& w) { figure(w, 1).nav->nodes[0].dir8 = 1; }},
        {"nodes[1]: a movement node is out of range",
         [](sim::World& w) { figure(w, 1).nav->nodes[1].length_q32 = 0; }},
        {"nodes[1]: a movement node is out of range",
         [](sim::World& w) { figure(w, 1).nav->nodes[1].dir8 = 8; }},
        {"nodes[1]: a movement node is out of range",
         [](sim::World& w) { figure(w, 1).nav->nodes[1].facing = 1; }},
        {"nodes[1]: a movement node is out of range",
         [](sim::World& w) { figure(w, 1).nav->nodes[1].length_q32 += 1; }},
        {"nodes[1]: a movement node is out of range",
         [](sim::World& w) { figure(w, 1).nav->nodes[1].length_q32 = 11 * sim::kNavDiagStepQ32; }},
        {"nodes[1]: a movement node is out of range",
         [](sim::World& w) { figure(w, 1).nav->nodes[1].speed_q32 = 0; }},
        {"nodes[1]: a movement node is out of range",
         [](sim::World& w) { figure(w, 1).nav->nodes[1].length_q32 = 11 * sim::kNavUnitQ32; }},
        {"nodes[1]: a movement node is out of range",
         [](sim::World& w) {
             figure(w, 1).nav->nodes[1].dir8 = 5;
             figure(w, 1).nav->nodes[1].length_q32 = 11 * sim::kNavDiagStepQ32;
         }},
        {"nodes[1]: a node is a movement or a turn",
         [](sim::World& w) { figure(w, 1).nav->nodes[1].kind = sim::NavSegKind::Idle; }},
        {"65537 pending nodes exceed the bound",
         [](sim::World& w) {
             figure(w, 1).nav->nodes.assign(
                 65537,
                 sim::NavNode{sim::NavSegKind::Move, 4, 0, sim::kNavUnitQ32, kFacts.speed_q32});
         }},
    };
    for (const Case& c : cases) {
        INFO(std::string(c.needle));
        sim::World w = busy;
        c.edit(w);
        const std::string problem = w.check_invariants();
        CHECK_MESSAGE(has(problem, c.needle), problem);
        // The refusal reaches the loader: a saved stream of the state is not loadable.
        auto refused = sim::World::load(w.save(), types);
        CHECK_FALSE(refused.ok());
    }
    // Valid controls close to the refusals: a residue just below 2^24 on a diagonal move, a
    // chunk of exactly 1.0, factor 1 and 100, facing 2*dir8 for every direction, a node of
    // exactly run_cap whole steps, an idle segment with exactly 0.01 left, a full turn step.
    for (std::uint8_t dir8 = 0; dir8 < 8; ++dir8) {
        sim::World w = busy;
        figure(w, 1).nav->seg.dir8 = dir8;
        figure(w, 1).dir16 = static_cast<std::uint8_t>(2 * dir8);
        if ((dir8 & 1U) == 0) {
            figure(w, 1).nav->res24[0] = figure(w, 1).nav->res24[1] = 0;
        }
        CHECK_MESSAGE(w.check_invariants().empty(), w.check_invariants());
    }
    for (const std::uint32_t factor : {1U, 100U}) {
        sim::World w = busy;
        figure(w, 1).nav->seg.factor_percent = factor;
        figure(w, 1).nav->seg.remaining_x500 = (1ULL << 32) * 500;
        CHECK(w.check_invariants().empty());
        figure(w, 1).nav->seg.remaining_x500 = sim::kNavChunkLimitQ32 * sim::kNavScale;
        CHECK(w.check_invariants().empty()); // a node up to 1.0 + f32(1e-4) is installed whole
    }
    {
        sim::World w = busy;
        figure(w, 1).nav->nodes[1].length_q32 = 10 * sim::kNavUnitQ32;
        figure(w, 1).nav->nodes.push_back(
            sim::NavNode{sim::NavSegKind::Move, 3, 0, 10 * sim::kNavDiagStepQ32, 1});
        figure(w, 1).nav->nodes.push_back(sim::NavNode{
            sim::NavSegKind::Turn, 0, 15, kFacts.turn_step_q32, sim::kNavIdleSpeedQ32});
        figure(w, 2).nav->seg.remaining_x500 = (1ULL << 32) * 500;
        CHECK_MESSAGE(w.check_invariants().empty(), w.check_invariants());
        figure(w, 2).nav->seg = sim::NavSegment{sim::NavSegKind::TurnPhase, 0, 100, 42949672U,
                                                kFacts.turn_step_q32 * 500ULL};
        CHECK_MESSAGE(w.check_invariants().empty(), w.check_invariants());
        figure(w, 1).pos.x = 1 << 22; // exactly the world bound
        figure(w, 1).nav->pos_u[0] = (std::int64_t{1} << 22) * sim::kNavPosUnitsPerQ8;
        sim::rebuild_figure_index(sim::WorldTestAccess::figures(w)); // the ship moved a cell
        CHECK_MESSAGE(w.check_invariants().empty(), w.check_invariants());
    }
}

TEST_CASE("nav motion: another ship type and other players carry the same state") {
    // Type 2001 navigates and owner 3/4 own the ships; 1502 is a ship-class type WITHOUT
    // navigation facts in this registry. Nothing in the state or its validation may depend on
    // the id 1502 or on player 0.
    const auto types = nav_registry(2001);
    CHECK_FALSE(types->find_figure_type(1502)->nav.has_value());
    REQUIRE(types->find_figure_type(2001)->nav.has_value());
    sim::ScenarioSpec spec = syn::empty_scenario(5);
    for (std::uint32_t p = 3; p < 5; ++p) {
        sim::ScenarioSpec::PlayerSpec ps;
        ps.id = sim::PlayerId{p};
        ps.money = 1;
        spec.players.push_back(ps);
    }
    for (std::uint32_t i = 0; i < 3; ++i) {
        sim::ScenarioSpec::FigureSpec f;
        f.kind = i == 2 ? kShip : 2001;
        f.owner = sim::PlayerId{3 + (i % 2)};
        f.pos = sim::FixedPos{static_cast<std::int32_t>(256 * (5 + i) + 1), 256 * 7 + 2};
        spec.figures.push_back(f);
    }
    auto created = sim::World::create(spec, types);
    REQUIRE_MESSAGE(created.ok(), (created.ok() ? std::string() : created.error()));
    sim::World w = std::move(created).value();
    CHECK(figure(w, 1).nav.has_value());
    CHECK(figure(w, 2).nav.has_value());
    CHECK_FALSE(figure(w, 3).nav.has_value());
    CHECK(w.check_invariants().empty());
    make_busy(w, 2);
    CHECK_MESSAGE(w.check_invariants().empty(), w.check_invariants());
    auto back = sim::World::load(w.save(), types);
    REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
    CHECK(back.value().state_hash() == w.state_hash());
    // The same state in a registry where 2001 does not navigate is refused with the figure named.
    auto other = sim::World::load(w.save(), nav_registry(kShip));
    CHECK_FALSE(other.ok());
}

TEST_CASE("nav motion: the wire bytes equal the hand transcription") {
    // Idle segment with one movement node (E, one diagonal step length, speed 154618816):
    // order none 00 | ticket none 00 | seg: kind 00, dir8 00, factor u32 100, speed u32 42949672 =
    // 0x028F5C28, remaining u64 42949672*500 = 0x4FFFFFE20 | node count u64 1 | node: kind 03, dir8
    // 04, facing 00, length u64 11863283<<9 = 0x16A09E600, speed u32 0x09374BC0 | last u16 0x0102 |
    // pos_u i64 1, i64 -1 | res24 two u32 0 | health 00 (absent; stream 123). (Python: tools/
    // wide_ref.py style big-int transcription; populated states: tools/nav_state_ref.py.)
    sim::NavMotion m = sim::make_nav_motion(sim::FixedPos{0, 0}, 0x0102);
    m.nodes = {sim::NavNode{sim::NavSegKind::Move, 4, 0, sim::kNavDiagStepQ32, 154618816U}};
    m.pos_u[0] = 1;
    m.pos_u[1] = -1;
    const std::vector<std::uint8_t> want1 = {
        0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x28, 0x5C, 0x8F, 0x02, 0x20, 0xFE,
        0xFF, 0xFF, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x03, 0x04, 0x00, 0x00, 0xE6, 0x09, 0x6A, 0x01, 0x00, 0x00, 0x00, 0xC0, 0x4B, 0x37,
        0x09, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    sim::WriteArchive w1;
    fields(w1, m);
    CHECK(w1.data() == want1);
    sim::ReadArchive r1(w1.data());
    sim::NavMotion back1;
    fields(r1, back1);
    CHECK_FALSE(r1.failed());
    CHECK(back1 == m);

    // Order and ticket present, a diagonal move, no nodes, the extreme stamp and residues:
    // order 01 | u64 7 | routed 01 | i32 -5 | i32 300 | ticket 01 | u64 9 | seg kind 03 dir8 03
    // factor 87 speed 154618816 remaining 2^32 | nodes 0 | last 0xFFFF | pos_u 500<<24,
    // -(500<<24)*3 | res24 12345, 0xFFFFFF | health 01 + u32 4321.
    sim::NavMotion m2;
    m2.health = 4321;
    m2.order = sim::NavOrder{7, true, -5, 300};
    m2.ticket = 9;
    m2.seg = sim::NavSegment{sim::NavSegKind::Move, 3, 87, 154618816U, std::uint64_t{1} << 32};
    m2.last = 0xFFFF;
    m2.pos_u[0] = std::int64_t{500} << 24;
    m2.pos_u[1] = -(std::int64_t{500} << 24) * 3;
    m2.res24[0] = 12345;
    m2.res24[1] = 0xFFFFFF;
    const std::vector<std::uint8_t> want2 = {
        0x01, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xFB, 0xFF, 0xFF, 0xFF,
        0x2C, 0x01, 0x00, 0x00, 0x01, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
        0x03, 0x57, 0x00, 0x00, 0x00, 0xC0, 0x4B, 0x37, 0x09, 0x00, 0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00,
        0x00, 0x00, 0xF4, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0xFA, 0xFF, 0xFF,
        0xFF, 0x39, 0x30, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0x01, 0xE1, 0x10, 0x00, 0x00};
    sim::WriteArchive w2;
    fields(w2, m2);
    CHECK(w2.data() == want2);
    sim::ReadArchive r2(w2.data());
    sim::NavMotion back2;
    fields(r2, back2);
    CHECK_FALSE(r2.failed());
    CHECK(back2 == m2);
}

TEST_CASE("nav motion: save, load, save is byte-identical and every field reaches the hash") {
    const auto types = nav_registry();
    sim::World w = nav_world(types);
    make_busy(w, 1);
    make_waiting(w, 2, 8, 9);
    REQUIRE(w.check_invariants().empty());
    const std::vector<std::uint8_t> bytes = w.save();
    auto loaded_world = sim::World::load(bytes, types);
    REQUIRE_MESSAGE(loaded_world.ok(), (loaded_world.ok() ? std::string() : loaded_world.error()));
    CHECK(loaded_world.value().save() == bytes);
    CHECK(loaded_world.value().state_hash() == w.state_hash());
    CHECK(*figure(loaded_world.value(), 1).nav == *figure(w, 1).nav);
    CHECK(loaded_world.value().ids().next_order_id == 100);
    CHECK(loaded_world.value().ids().next_ticket == 50);

    // Every persisted member changes the hash (each edit stays a valid state).
    const std::uint64_t base = w.state_hash();
    struct Edit {
        const char* name;
        void (*apply)(sim::World&);
    };
    const Edit edits[] = {
        {"order_id", [](sim::World& x) { figure(x, 1).nav->order->order_id = 20; }},
        {"target x", [](sim::World& x) { figure(x, 1).nav->order->target_x_q8 += 1; }},
        {"target y", [](sim::World& x) { figure(x, 1).nav->order->target_y_q8 += 1; }},
        {"order absent",
         [](sim::World& x) {
             figure(x, 1).nav->order.reset();
             figure(x, 1).nav->ticket.reset();
             figure(x, 1).nav->nodes.clear();
         }},
        {"ticket", [](sim::World& x) { *figure(x, 2).nav->ticket = 10; }},
        {"waiting order id", [](sim::World& x) { figure(x, 2).nav->order->order_id = 21; }},
        {"waiting target", [](sim::World& x) { figure(x, 2).nav->order->target_y_q8 = -1; }},
        {"order consumed its ticket",
         [](sim::World& x) {
             figure(x, 2).nav->order->routed = true;
             figure(x, 2).nav->ticket.reset();
         }},
        {"seg dir8",
         [](sim::World& x) {
             figure(x, 1).nav->seg.dir8 = 1;
             figure(x, 1).dir16 = 2;
         }},
        {"turn node facing", [](sim::World& x) { figure(x, 1).nav->nodes[0].facing = 5; }},
        {"seg factor", [](sim::World& x) { figure(x, 1).nav->seg.factor_percent = 88; }},
        {"seg remaining", [](sim::World& x) { figure(x, 1).nav->seg.remaining_x500 -= 1; }},
        {"seg speed", [](sim::World& x) { figure(x, 1).nav->seg.speed_q32 -= 1; }},
        {"seg kind",
         [](sim::World& x) {
             figure(x, 1).nav->seg.kind = sim::NavSegKind::Idle;
             figure(x, 1).nav->seg.speed_q32 = sim::kNavIdleSpeedQ32;
             figure(x, 1).nav->seg.remaining_x500 = sim::kNavIdleSpeedQ32 * 500ULL;
             figure(x, 1).nav->seg.factor_percent = 100;
             figure(x, 1).nav->seg.dir8 = 0;
             figure(x, 1).nav->res24[0] = figure(x, 1).nav->res24[1] = 0;
         }},
        {"node dropped", [](sim::World& x) { figure(x, 1).nav->nodes.pop_back(); }},
        {"node length",
         [](sim::World& x) { figure(x, 1).nav->nodes[1].length_q32 += sim::kNavUnitQ32; }},
        {"node speed", [](sim::World& x) { figure(x, 1).nav->nodes[1].speed_q32 += 1; }},
        {"node dir", [](sim::World& x) { figure(x, 1).nav->nodes[1].dir8 = 6; }},
        {"last", [](sim::World& x) { figure(x, 1).nav->last += 1; }},
        {"pos_u x", [](sim::World& x) { figure(x, 1).nav->pos_u[0] += 1; }},
        {"pos_u y", [](sim::World& x) { figure(x, 1).nav->pos_u[1] -= 1; }},
        {"res24 x", [](sim::World& x) { figure(x, 1).nav->res24[0] += 1; }},
        {"res24 y", [](sim::World& x) { figure(x, 1).nav->res24[1] -= 1; }},
        {"order counter", [](sim::World& x) { sim::WorldTestAccess::ids(x).next_order_id += 1; }},
        {"idle remaining", [](sim::World& x) { figure(x, 2).nav->seg.remaining_x500 -= 1; }},
        {"ticket counter", [](sim::World& x) { sim::WorldTestAccess::ids(x).next_ticket += 1; }},
    };
    std::vector<std::uint64_t> seen{base};
    for (const Edit& e : edits) {
        INFO(e.name);
        sim::World x = w;
        e.apply(x);
        sim::rebuild_figure_index(sim::WorldTestAccess::figures(x)); // derived indexes
        CHECK_MESSAGE(x.check_invariants().empty(), x.check_invariants());
        const std::uint64_t h = x.state_hash();
        CHECK(h != base);
        CHECK(std::find(seen.begin(), seen.end(), h) == seen.end());
        seen.push_back(h);
    }
    // The previous stream versions are refused with their own message, a current stream133 control
    // loads (122 is the layout before NavMotion::health; the control is this build's valid,
    // checksummed save with only the number changed and re-sealed, and the 122 message names 122).
    island_test::check_unsupported_versions(
        bytes, types, {116, 117, 118, 119, 120, 121, 122, 123, 125, 126, 127});
    CHECK(sim::kStateHashVersion == 137);
}

TEST_CASE("nav motion: make_nav_motion is the documented idle state at any position") {
    for (const sim::FixedPos p :
         {sim::FixedPos{0, 0}, sim::FixedPos{-1, 1}, sim::FixedPos{(1 << 22), -(1 << 22)}}) {
        const sim::NavMotion m = sim::make_nav_motion(p, 65535);
        CHECK(m.seg.kind == sim::NavSegKind::Idle);
        CHECK(m.seg.speed_q32 == 42949672U);
        CHECK(m.seg.remaining_x500 == 42949672ULL * 500);
        CHECK(m.seg.factor_percent == 100U);
        CHECK(m.last == 65535);
        CHECK(m.pos_u[0] == std::int64_t{p.x} * 8388608000LL); // 500 << 24
        CHECK(m.pos_u[1] == std::int64_t{p.y} * 8388608000LL);
        CHECK(m.res24[0] == 0);
        CHECK(m.res24[1] == 0);
    }
    // The constants the contract fixes: 0.01f as q32 and the diagonal step.
    CHECK(sim::kNavIdleSpeedQ32 == 42949672U);
    CHECK(sim::kNavDiagStepQ32 == 6074000896ULL);
    CHECK(sim::kNavUnitQ32 == 4294967296ULL);
}

TEST_CASE("nav motion: a ship outside the configured world is refused before any motion state") {
    // pos * 500 * 2^24 must stay inside i64: the scenario's world-size check (existing) keeps
    // every navigating ship far below 2^22 Q8, and validate_navigation bounds a loaded state by
    // the absolute world bound (cases above).
    const auto types = nav_registry();
    for (const std::int32_t v : {std::numeric_limits<std::int32_t>::max(),
                                 std::numeric_limits<std::int32_t>::min(), (1 << 22) + 1}) {
        for (const sim::FixedPos pos : {sim::FixedPos{v, 0}, sim::FixedPos{0, v}}) {
            sim::ScenarioSpec spec = syn::empty_scenario(1);
            sim::ScenarioSpec::PlayerSpec ps;
            ps.id = sim::PlayerId{0};
            spec.players.push_back(ps);
            sim::ScenarioSpec::FigureSpec f;
            f.kind = kShip;
            f.owner = sim::PlayerId{0};
            f.pos = pos;
            spec.figures.push_back(f);
            auto r = sim::World::create(spec, types);
            REQUIRE_FALSE(r.ok());
            CHECK(has(r.error(), "figures[0].pos"));
        }
    }
}

TEST_CASE("nav motion: a navigation class the Rules table lacks is refused when a ship exists") {
    // Default Rules hold four classes (0..3). A registry whose ship type uses class 3 is fine,
    // class 4 names the type and both numbers.
    sim::NavFacts f = kFacts;
    f.nav_class = 3;
    sim::World ok = nav_world(nav_registry(kShip, f));
    CHECK(ok.check_invariants().empty());
    f.nav_class = 4;
    sim::ScenarioSpec spec = syn::empty_scenario(2);
    sim::ScenarioSpec::PlayerSpec ps;
    ps.id = sim::PlayerId{0};
    spec.players.push_back(ps);
    sim::ScenarioSpec::FigureSpec fs;
    fs.kind = kShip;
    fs.owner = sim::PlayerId{0};
    fs.pos = sim::FixedPos{256 * 10, 256 * 10};
    spec.figures.push_back(fs);
    const auto types4 = nav_registry(kShip, f);
    auto created = sim::World::create(spec, types4);
    // create may refuse or yield a world whose invariants name the class; either names it.
    const std::string problem = created.ok() ? created.value().check_invariants() : created.error();
    CHECK(has(problem, "has nav_class 4, Rules.nav.class_costs holds 4"));
}

TEST_CASE("nav motion: real data - type 391 navigates with no holds, the factor rule on Rules") {
    // (the real-file assertion for 391 is in founding_assets_test; here the Rules side)
    struct Pair {
        std::uint32_t floor_pct, penalty;
        bool valid;
    };
    // floor * (100 - penalty) >= 100 keeps V = trunc(floor * L / 100) >= 1 for L >= 100 - penalty.
    for (const Pair p :
         {Pair{65, 17, true}, Pair{65, 98, true}, Pair{65, 99, false}, Pair{1, 0, true},
          Pair{1, 1, false}, Pair{1, 99, false}, Pair{50, 98, true}, Pair{50, 99, false},
          Pair{100, 99, true}, Pair{100, 100, false}, Pair{99, 100, false}}) {
        sim::Rules r;
        r.nav.health_floor_percent = p.floor_pct;
        r.nav.health_span_percent = 0;
        r.nav.cargo_penalty_percent = p.penalty;
        const std::string err = r.validate();
        INFO(p.floor_pct << " " << p.penalty);
        if (p.valid) {
            CHECK(err.empty());
        } else {
            CHECK(has(err, "cargo_penalty_percent"));
        }
    }
}

namespace {

/// One state of nav_state_vectors.txt (written by tools/nav_state_ref.py).
struct NavVector {
    std::string name;
    std::int32_t pos_x = 0, pos_y = 0;
    std::uint8_t dir16 = 0;
    sim::NavMotion motion;
    std::string hex;
};

std::vector<NavVector> load_nav_vectors() {
    std::ifstream in(std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "nav" /
                     "nav_state_vectors.txt");
    REQUIRE(in.good());
    std::vector<NavVector> out;
    const auto kind_of = [](const std::string& k) {
        if (k == "Idle") {
            return sim::NavSegKind::Idle;
        }
        if (k == "Turn") {
            return sim::NavSegKind::Turn;
        }
        if (k == "TurnPhase") {
            return sim::NavSegKind::TurnPhase;
        }
        REQUIRE(k == "Move");
        return sim::NavSegKind::Move;
    };
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream ls(line);
        std::string key;
        ls >> key;
        if (key == "state") {
            out.emplace_back();
            ls >> out.back().name;
            continue;
        }
        REQUIRE_FALSE(out.empty());
        NavVector& v = out.back();
        sim::NavMotion& m = v.motion;
        if (key == "pos") {
            ls >> v.pos_x >> v.pos_y;
        } else if (key == "dir16") {
            unsigned d = 0;
            ls >> d;
            v.dir16 = static_cast<std::uint8_t>(d);
        } else if (key == "order") {
            std::string first;
            ls >> first;
            if (first != "none") {
                sim::NavOrder o;
                unsigned routed = 0;
                o.order_id = std::stoull(first);
                ls >> routed >> o.target_x_q8 >> o.target_y_q8;
                o.routed = routed != 0;
                m.order = o;
            }
        } else if (key == "ticket") {
            std::string t;
            ls >> t;
            if (t != "none") {
                m.ticket = std::stoull(t);
            }
        } else if (key == "seg") {
            std::string k;
            unsigned dir = 0;
            ls >> k >> dir >> m.seg.factor_percent >> m.seg.speed_q32 >> m.seg.remaining_x500;
            m.seg.kind = kind_of(k);
            m.seg.dir8 = static_cast<std::uint8_t>(dir);
        } else if (key == "node") {
            std::string k;
            unsigned dir = 0, facing = 0;
            sim::NavNode n;
            ls >> k >> dir >> facing >> n.length_q32 >> n.speed_q32;
            n.kind = kind_of(k);
            n.dir8 = static_cast<std::uint8_t>(dir);
            n.facing = static_cast<std::uint8_t>(facing);
            m.nodes.push_back(n);
        } else if (key == "last") {
            unsigned l = 0;
            ls >> l;
            m.last = static_cast<std::uint16_t>(l);
        } else if (key == "pos_u") {
            ls >> m.pos_u[0] >> m.pos_u[1];
        } else if (key == "res24") {
            ls >> m.res24[0] >> m.res24[1];
        } else if (key == "health") {
            std::string h;
            ls >> h;
            if (h != "unknown") {
                m.health = static_cast<std::uint32_t>(std::stoul(h));
            }
        } else if (key == "hex") {
            ls >> v.hex;
        }
    }
    return out;
}

std::string hex_of(const std::vector<std::uint8_t>& bytes) {
    static const char* d = "0123456789abcdef";
    std::string s;
    for (const std::uint8_t b : bytes) {
        s.push_back(d[b >> 4]);
        s.push_back(d[b & 15]);
    }
    return s;
}

std::size_t occurrences(const std::string& hay, const std::string& needle) {
    std::size_t n = 0;
    for (std::size_t at = hay.find(needle); at != std::string::npos;
         at = hay.find(needle, at + 2)) {
        // a byte boundary only (even offset): hex digits of two adjacent bytes may look alike
        n += (at % 2 == 0) ? 1U : 0U;
    }
    return n;
}

} // namespace

TEST_CASE("nav state: populated states serialise to the independent byte vectors, then sail on") {
    const std::vector<NavVector> vectors = load_nav_vectors();
    REQUIRE(vectors.size() == 5);
    const auto types = nav_registry();
    for (const NavVector& v : vectors) {
        INFO(v.name);
        // 1. The production archive writes exactly the bytes of tools/nav_state_ref.py.
        sim::WriteArchive wa;
        sim::NavMotion copy = v.motion;
        fields(wa, copy);
        CHECK_MESSAGE(hex_of(wa.data()) == v.hex, "the writer differs from nav_state_vectors.txt");

        // 2. The same state in a world: valid, and its save holds those bytes once, as the
        // presence-prefixed optional of ship 1's Figure::nav.
        sim::World w = nav_world(types);
        sim::IdCounters& ids = sim::WorldTestAccess::ids(w);
        ids.next_order_id = 100;
        ids.next_ticket = 50;
        sim::Figure& f = figure(w, 1);
        f.pos = sim::FixedPos{v.pos_x, v.pos_y};
        f.dir16 = v.dir16;
        f.nav = v.motion;
        sim::rebuild_figure_index(sim::WorldTestAccess::figures(w));
        REQUIRE_MESSAGE(w.check_invariants().empty(), w.check_invariants());
        const std::vector<std::uint8_t> saved = w.save();
        CHECK(occurrences(hex_of(saved), "01" + v.hex) >= 1);
        CHECK(occurrences(hex_of(saved), v.hex) >= 1);

        // 3. The loaded world is the same state and sails on exactly like the continuous one.
        auto loaded = sim::World::load(saved, types);
        REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
        sim::World r = std::move(loaded).value();
        REQUIRE(r.state_hash() == w.state_hash());
        CHECK(*figure(r, 1).nav == v.motion);
        const sim::FixedPos start = f.pos;
        std::vector<std::uint32_t> later_factors;
        for (int step = 0; step < 600; ++step) {
            w.advance(10);
            r.advance(10);
            REQUIRE(r.state_hash() == w.state_hash());
            if (step >= 3 && figure(w, 1).nav->seg.kind == sim::NavSegKind::Move) {
                later_factors.push_back(figure(w, 1).nav->seg.factor_percent);
            }
        }
        CHECK(w.check_invariants().empty());
        CHECK(r.save() == w.save());
        if (v.motion.order.has_value()) {
            // The voyage really happened: the ship left its place, or arrived and the order is
            // gone (a waiting order is planned by the fair admission, then sailed).
            CHECK((figure(w, 1).pos.x != start.x || figure(w, 1).pos.y != start.y));
        }
        // The factor of the segments that START after the load follows the stored health
        // (6000 of 6000: 100; 1500 of 6000: 65 + trunc(35 * 1500 / 6000) = 73; unknown: 100).
        const std::uint32_t expect =
            v.motion.health == std::optional<std::uint32_t>{1500} ? 73 : 100;
        for (const std::uint32_t factor : later_factors) {
            CHECK(factor == expect);
        }
        if (v.name == "diag_moving_full" || v.name == "diag_moving_partial") {
            CHECK_FALSE(later_factors.empty());
        }
    }
}
