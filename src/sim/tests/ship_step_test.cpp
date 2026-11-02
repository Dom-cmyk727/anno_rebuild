// The ship stepper (task MAP-NAV-2 package D; docs/sim/ship-navigation.md 12.4). The vector file
// fixtures/synthetic/sim/nav/stepper_vectors.txt comes from tools/ship_step_ref.py, an
// exact-rational model that shares no algorithm with the integer stepper; the hand-derived cases
// below do not use it.
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/archive.hpp>
#include <anno/sim/ship_step.hpp>
#include <anno/sim/state_fields.hpp>

#include "ship_helpers.hpp"

namespace sim = anno::sim;

namespace {

constexpr std::uint32_t kTurn = 472446400U; // type 1502 f0 0.11 as q32
constexpr std::uint32_t kSpeed = 154618816U;
constexpr std::uint64_t kUnit = sim::kNavUnitQ32;

struct Case {
    std::string name;
    std::uint32_t v = 100, turn = kTurn;
    std::uint16_t last = 0;
    std::int32_t px = 0, py = 0;
    std::vector<sim::NavNode> nodes;
    std::vector<std::uint32_t> visits;
    std::size_t arrive = 0;
    std::int64_t tol = 0;
    std::size_t visitcount = 0;
    std::map<std::size_t, std::pair<std::int64_t, std::int64_t>> at;
};

std::vector<Case> load_cases() {
    std::ifstream in(std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "nav" /
                     "stepper_vectors.txt");
    REQUIRE(in.good());
    std::vector<Case> cases;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream ls(line);
        std::string key;
        ls >> key;
        if (key == "case") {
            cases.emplace_back();
            ls >> cases.back().name;
            continue;
        }
        REQUIRE_FALSE(cases.empty());
        Case& c = cases.back();
        if (key == "v") {
            ls >> c.v;
        } else if (key == "turn") {
            ls >> c.turn;
        } else if (key == "last") {
            unsigned l = 0;
            ls >> l;
            c.last = static_cast<std::uint16_t>(l);
        } else if (key == "pos") {
            ls >> c.px >> c.py;
        } else if (key == "nodes") {
            std::string tok;
            while (ls >> tok) {
                sim::NavNode n;
                if (tok[0] == 'T') {
                    n.kind = sim::NavSegKind::Turn;
                    n.facing = static_cast<std::uint8_t>(std::stoi(tok.substr(2)));
                    n.length_q32 = c.turn;
                    n.speed_q32 = sim::kNavIdleSpeedQ32;
                } else {
                    unsigned d = 0;
                    unsigned long long len = 0, sp = 0;
                    REQUIRE(std::sscanf(tok.c_str(), "M:%u:%llu:%llu", &d, &len, &sp) == 3);
                    n.kind = sim::NavSegKind::Move;
                    n.dir8 = static_cast<std::uint8_t>(d);
                    n.length_q32 = len;
                    n.speed_q32 = static_cast<std::uint32_t>(sp);
                }
                c.nodes.push_back(n);
            }
        } else if (key == "visits") {
            std::uint32_t v = 0;
            while (ls >> v) {
                c.visits.push_back(v);
            }
        } else if (key == "arrive") {
            ls >> c.arrive;
        } else if (key == "tol") {
            ls >> c.tol;
        } else if (key == "visitcount") {
            ls >> c.visitcount;
        } else if (key == "at") {
            std::size_t i = 0;
            std::int64_t dx = 0, dy = 0;
            ls >> i >> dx >> dy;
            c.at[i] = {dx, dy};
        }
    }
    return cases;
}

sim::NavFacts facts_with(std::uint32_t turn) {
    return sim::NavFacts{kSpeed, turn, 6000, 4, 2, 0, 1280};
}

/// A ship that has just committed `nodes`: routed order, zero-length first segment, the facing set
/// to the first movement, moving bit set (docs/re/ship-navigation.md 8).
sim::Figure committed_ship(const Case& c) {
    sim::Figure f;
    f.pos = sim::FixedPos{c.px, c.py};
    f.dir16 = 5; // anything: the commit overwrites it
    f.nav = sim::make_nav_motion(f.pos, c.last);
    sim::NavMotion& m = *f.nav;
    m.order = sim::NavOrder{7, true, 0, 0};
    m.nodes = c.nodes;
    m.seg.remaining_x500 = 0;
    const auto first = std::find_if(c.nodes.begin(), c.nodes.end(), [](const sim::NavNode& n) {
        return n.kind == sim::NavSegKind::Move;
    });
    REQUIRE(first != c.nodes.end());
    f.dir16 = sim::nav_facing_of_dir8(first->dir8);
    f.state_word |= 1;
    return f;
}

class NoPlanner : public sim::NavPlanner {
  public:
    sim::NavPlan plan(const sim::Figure&) override {
        ++calls;
        return {};
    }
    int calls = 0;
};

/// The navigation validator's verdict on one figure (the same check the world runs).
std::string nav_problem(sim::Figure f) {
    static const auto types = ship_test::nav_ship_registry();
    const sim::SimConfig cfg;
    const sim::Rules rules;
    sim::FigureRegistry reg = sim::make_figure_registry(cfg);
    f.id = sim::FigureId{1};
    f.kind = ship_test::kShip;
    sim::IdCounters ids;
    ids.next_figure = 2;
    ids.next_order_id = 1000;
    ids.next_ticket = 1000;
    if (sim::add_figure(reg, f, cfg, rules) == sim::kNoSlot) {
        return "add_figure failed";
    }
    return sim::validate_navigation(reg, ids, *types, rules);
}

std::uint16_t now_of(std::uint16_t last, std::uint64_t cum) {
    return static_cast<std::uint16_t>((last + cum) & 0xFFFFU);
}

std::vector<std::uint8_t> motion_bytes(const sim::Figure& f) {
    sim::WriteArchive w;
    sim::NavMotion m = *f.nav;
    fields(w, m);
    std::vector<std::uint8_t> b = w.data();
    for (const std::int32_t v : {f.pos.x, f.pos.y, static_cast<std::int32_t>(f.dir16),
                                 static_cast<std::int32_t>(f.state_word)}) {
        for (int i = 0; i < 4; ++i) {
            b.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(v) >> (8 * i)));
        }
    }
    return b;
}

} // namespace

TEST_CASE(
    "stepper: every rational-model vector, position after each visit within the stated tolerance") {
    const std::vector<Case> cases = load_cases();
    REQUIRE(cases.size() >= 25);
    for (const Case& c : cases) {
        INFO(c.name);
        REQUIRE(c.visits.size() == c.visitcount);
        sim::Figure f = committed_ship(c);
        const sim::NavFacts facts = facts_with(c.turn);
        const sim::NavStepEnv env{&facts, c.v};
        NoPlanner planner;
        const std::int64_t x0 = f.nav->pos_u[0], y0 = f.nav->pos_u[1];
        std::uint64_t cum = 0;
        std::size_t arrived_at = 0, arrived_count = 0;
        std::int64_t worst = 0;
        for (std::size_t i = 1; i <= c.visits.size(); ++i) {
            cum += c.visits[i - 1];
            std::vector<sim::NavEvent> ev;
            sim::nav_step(f, env, planner, now_of(c.last, cum), ev);
            for (const sim::NavEvent& e : ev) {
                CHECK(e.kind == sim::NavEventKind::Arrived);
                CHECK(e.order_id == 7);
                ++arrived_count;
                arrived_at = arrived_at == 0 ? i : arrived_at;
            }
            const auto it = c.at.find(i);
            if (it != c.at.end()) {
                const std::int64_t ex = f.nav->pos_u[0] - x0, ey = f.nav->pos_u[1] - y0;
                const std::int64_t err =
                    std::max(std::llabs(ex - it->second.first), std::llabs(ey - it->second.second));
                worst = std::max(worst, err);
                CHECK_MESSAGE(err <= c.tol, "visit " << i << " dx " << ex << " want "
                                                     << it->second.first << " dy " << ey << " want "
                                                     << it->second.second);
            }
            CHECK(f.nav->res24[0] < (1U << 24));
            if (i % 11 == 0 || i == c.visits.size()) {
                const std::string problem = nav_problem(f);
                CHECK_MESSAGE(problem.empty(), "visit " << i << ": " << problem);
            }
        }
        CHECK(arrived_count == 1);
        CHECK(arrived_at == c.arrive);
        CHECK(planner.calls == 0);
        // The ship idles at the end: order gone, no nodes, not moving.
        CHECK_FALSE(f.nav->order.has_value());
        CHECK(f.nav->nodes.empty());
        CHECK((f.state_word & 1U) == 0);
        MESSAGE(c.name << ": worst error " << worst << " of tolerance " << c.tol);
    }
}

TEST_CASE("stepper: a segment ending exactly on a visit is exact (hand-derived)") {
    // Speed 2^26 q32 per stepper unit, V 100: per tick 2^26 * 100 / 500 q32; one 1.0 unit takes
    // 2^32 * 500 / (2^26 * 100) = 320 ticks. A 320-tick visit ends it exactly: position 2^32 * 500
    // units, no error, arrival at that visit; the ship then idles with remaining 1.0.
    Case c;
    c.nodes = {sim::NavNode{sim::NavSegKind::Move, 4, 0, kUnit, 1U << 26}};
    sim::Figure f = committed_ship(c);
    const sim::NavFacts facts = facts_with(kTurn);
    NoPlanner planner;
    std::vector<sim::NavEvent> ev;
    sim::nav_step(f, sim::NavStepEnv{&facts, 100}, planner, 320, ev);
    REQUIRE(ev.size() == 1);
    CHECK(f.nav->pos_u[0] == std::int64_t{4294967296LL} * 500);
    CHECK(f.nav->pos_u[1] == 0);
    CHECK(f.pos.x == 256); // 2^32 q32 = 1 tile = 256 Q24.8 units
    CHECK(f.nav->seg.kind == sim::NavSegKind::Idle);
    CHECK(f.nav->seg.remaining_x500 == (std::uint64_t{1} << 32) * 500);
    CHECK(f.nav->last == 320);
    // 319 ticks: one tick short, 319 * 2^26 * 100 units, no arrival.
    sim::Figure g = committed_ship(c);
    ev.clear();
    sim::nav_step(g, sim::NavStepEnv{&facts, 100}, planner, 319, ev);
    CHECK(ev.empty());
    CHECK(g.nav->pos_u[0] == std::int64_t{319} * (std::int64_t{1} << 26) * 100);
    CHECK(g.nav->seg.remaining_x500 ==
          (std::uint64_t{1} << 32) * 500 - 319ULL * (std::uint64_t{1} << 26) * 100);
}

TEST_CASE("stepper: a zero-tick visit changes nothing, a wrapped stamp counts forward") {
    Case c;
    c.last = 65535;
    c.nodes = {sim::NavNode{sim::NavSegKind::Move, 2, 0, 3 * kUnit, kSpeed}};
    sim::Figure f = committed_ship(c);
    const sim::NavFacts facts = facts_with(kTurn);
    NoPlanner planner;
    std::vector<sim::NavEvent> ev;
    const auto before = motion_bytes(f);
    sim::nav_step(f, sim::NavStepEnv{&facts, 100}, planner, 65535, ev); // d = 0
    CHECK(motion_bytes(f) == before);
    // now = 4 after last = 65535: u16 difference 5 ticks, y grows by 5 * speed * 100 units.
    sim::nav_step(f, sim::NavStepEnv{&facts, 100}, planner, 4, ev);
    CHECK(f.nav->last == 4);
    CHECK(f.nav->pos_u[1] == 5LL * kSpeed * 100);
    CHECK(f.nav->pos_u[0] == 0);
    // the whole 16-bit range of a visit: 65535 ticks
    // (a slow node, 9 units at 2^20 q32 per stepper unit, so the whole visit is spent sailing)
    c.nodes = {sim::NavNode{sim::NavSegKind::Move, 2, 0, 9 * kUnit, 1U << 20}};
    sim::Figure g = committed_ship(c);
    sim::nav_step(g, sim::NavStepEnv{&facts, 100}, planner, 65534, ev);
    CHECK(g.nav->last == 65534);
    CHECK(g.nav->pos_u[1] == 65535LL * (1LL << 20) * 100);
}

TEST_CASE(
    "stepper: turns follow the d >= 3 rule, a reversal turns -1, the facing changes at once") {
    const sim::NavFacts facts = facts_with(kTurn);
    NoPlanner planner;
    // Facing 8 (E, dir8 4) to W (dir8 0, want 0): d_signed 8, d 8, six phases, facings 7 6 5 4 3 2
    // and the snap to 0 (docs/re/ship-navigation.md 7.3, emulated on the real bytes).
    Case c;
    c.nodes = {sim::NavNode{sim::NavSegKind::Move, 4, 0, kUnit, kSpeed},
               sim::NavNode{sim::NavSegKind::Move, 0, 0, kUnit, kSpeed}};
    sim::Figure f = committed_ship(c);
    CHECK(f.dir16 == 8);
    // Run the first segment out (1.0 unit), then watch the turn phases one by one.
    std::vector<sim::NavEvent> ev;
    std::uint64_t cum = 0;
    const auto visit = [&](std::uint32_t ticks) {
        cum += ticks;
        sim::nav_step(f, sim::NavStepEnv{&facts, 100}, planner, now_of(0, cum), ev);
    };
    visit(1); // zero-length commit segment, the E node starts: facing 8, Move
    CHECK(f.nav->seg.kind == sim::NavSegKind::Move);
    // 1.0 unit at 0.036 tile per unit = 27.78 units = 138.9 ticks: after 140 ticks it ended, the W
    // node started and its first phase turned the facing to 7 immediately.
    visit(139);
    CHECK(f.nav->seg.kind == sim::NavSegKind::TurnPhase);
    CHECK(f.dir16 == 7);
    CHECK((f.state_word & 1U) == 1U);
    const std::uint8_t expected_facings[] = {6, 5, 4, 3, 2};
    for (const std::uint8_t want : expected_facings) {
        // one phase = turn_step * 500 / (idle speed * 100) ticks = 55 ticks for 0.11 (exact: 55.0)
        visit(55);
        CHECK(f.dir16 == want);
        CHECK(f.nav->seg.kind == sim::NavSegKind::TurnPhase);
    }
    visit(55); // the sixth phase ends: the W node starts with the snap to 0
    CHECK(f.dir16 == 0);
    CHECK(f.nav->seg.kind == sim::NavSegKind::Move);
    CHECK(f.nav->seg.dir8 == 0);
}

TEST_CASE(
    "stepper: a chunk is cut above 1.0 + f32(1e-4) and the speed factor is read at segment start") {
    const sim::NavFacts facts = facts_with(kTurn);
    NoPlanner planner;
    const auto first_segment = [&](std::uint64_t length, std::uint32_t v) {
        Case c;
        c.nodes = {sim::NavNode{sim::NavSegKind::Move, 4, 0, length, kSpeed}};
        sim::Figure f = committed_ship(c);
        std::vector<sim::NavEvent> ev;
        sim::nav_step(f, sim::NavStepEnv{&facts, v}, planner, 1, ev);
        return f;
    };
    // f32(1e-4) = 0xD1B717 * 2^-37 = 13743895 / 2^37 (struct-derived in tools/ship_step_ref.py) =
    // 429496.72 q32: a length of 2^32 + 429496 is installed whole, 2^32 + 429497 is cut into 1.0
    // and 429497.
    const sim::Figure whole = first_segment(kUnit + 429496, 100);
    CHECK(whole.nav->nodes.empty());
    CHECK(whole.nav->seg.kind == sim::NavSegKind::Move);
    const sim::Figure cut = first_segment(kUnit + 429497, 100);
    REQUIRE(cut.nav->nodes.size() == 1);
    CHECK(cut.nav->nodes[0].length_q32 == 429497);
    // The factor V of the start is stored in the segment and the rate is speed * V.
    const sim::Figure slow = first_segment(5 * kUnit, 65);
    CHECK(slow.nav->seg.factor_percent == 65);
    CHECK(slow.nav->pos_u[0] == 1LL * kSpeed * 65); // the one tick of the visit
}

TEST_CASE("stepper: speed change inside a visit converts the leftover time with one floor") {
    // Segment A: 1.0 unit at speed 2^26 (V 100): 320 ticks. Segment B: speed 3 * 2^26. A 400-tick
    // visit: A ends at 320, 80 ticks remain: B moves 80 * 3 * 2^26 * 100 units exactly (the rates
    // are 2^26 * 100 and 3 * 2^26 * 100, leftover 80 * rate_A converts to 80 * rate_B, no
    // remainder).
    Case c;
    c.nodes = {sim::NavNode{sim::NavSegKind::Move, 4, 0, kUnit, 1U << 26},
               sim::NavNode{sim::NavSegKind::Move, 4, 0, 5 * kUnit, 3U << 26}};
    sim::Figure f = committed_ship(c);
    const sim::NavFacts facts = facts_with(kTurn);
    NoPlanner planner;
    std::vector<sim::NavEvent> ev;
    sim::nav_step(f, sim::NavStepEnv{&facts, 100}, planner, 400, ev);
    CHECK(f.nav->pos_u[0] == std::int64_t{4294967296LL} * 500 + 80LL * 3 * (1LL << 26) * 100);
    // A leftover that does not divide: segment A 7 ticks short of a rate change into 3 * 2^26 / 7:
    Case d;
    d.nodes = {sim::NavNode{sim::NavSegKind::Move, 4, 0, kUnit, 1U << 26},
               sim::NavNode{sim::NavSegKind::Move, 4, 0, 5 * kUnit, 7U}};
    sim::Figure g = committed_ship(d);
    ev.clear();
    sim::nav_step(g, sim::NavStepEnv{&facts, 100}, planner, 321, ev);
    // 321 ticks: 1 tick leftover at rate A = 2^26 * 100, new rate 7 * 100: floor(2^26 * 100 * 700 /
    // (2^26 * 100)) = 700
    CHECK(g.nav->pos_u[0] == std::int64_t{4294967296LL} * 500 + 700);
    Case e;
    e.nodes = {sim::NavNode{sim::NavSegKind::Move, 4, 0, kUnit, (1U << 26) + 1},
               sim::NavNode{sim::NavSegKind::Move, 4, 0, 5 * kUnit, 7U}};
    sim::Figure h = committed_ship(e);
    ev.clear();
    // rate A = (2^26 + 1) * 100: 320 ticks give 320 * (2^26+1) * 100 = 2^32 * 500 * (1 + 320/2^26
    // ... ) > unit, so A ends before tick 320; the leftover is converted by floor(leftover * 700 /
    // rate_A).
    sim::nav_step(h, sim::NavStepEnv{&facts, 100}, planner, 320, ev);
    const std::uint64_t rate_a = ((1ULL << 26) + 1) * 100;
    const std::uint64_t unit500 = (1ULL << 32) * 500;
    const std::uint64_t leftover = 320 * rate_a - unit500;
    CHECK(h.nav->pos_u[0] == static_cast<std::int64_t>(unit500 + leftover * 700 / rate_a));
}

TEST_CASE("stepper: diagonal movement keeps the residue, the shortfall of 2 c^2 and resets it at "
          "the end") {
    const sim::NavFacts facts = facts_with(kTurn);
    NoPlanner planner;
    Case c;
    c.nodes = {sim::NavNode{sim::NavSegKind::Move, 3, 0, sim::kNavDiagStepQ32, 1U << 26},
               sim::NavNode{sim::NavSegKind::Move, 3, 0, sim::kNavDiagStepQ32, 1U << 26}};
    sim::Figure f = committed_ship(c);
    std::vector<sim::NavEvent> ev;
    // 7 ticks into the first segment: step = 7 * 2^26 * 100 units; per axis floor(step * c) with
    // the remainder in res24 (hand: 7 * 2^26 * 100 * 11863283 / 2^24 = 7 * 4 * 100 * 11863283 =
    // 33217192400, exact)
    sim::nav_step(f, sim::NavStepEnv{&facts, 100}, planner, 7, ev);
    CHECK(f.nav->pos_u[0] == 33217192400LL);
    CHECK(f.nav->pos_u[1] == 33217192400LL);
    CHECK(f.nav->res24[0] == 0);
    // One tick of a speed whose product is not a multiple of 2^24: speed 1: step 100 per tick:
    // 100 * 11863283 = 1186328300 = 70 * 2^24 + 11863283 * 100 mod 2^24
    Case d;
    d.nodes = {sim::NavNode{sim::NavSegKind::Move, 3, 0, sim::kNavDiagStepQ32, 1U}};
    sim::Figure g = committed_ship(d);
    sim::nav_step(g, sim::NavStepEnv{&facts, 100}, planner, 1, ev);
    CHECK(g.nav->pos_u[0] == 1186328300LL / (1LL << 24));
    CHECK(g.nav->res24[0] == 1186328300ULL % (1ULL << 24));
    CHECK(g.nav->res24[1] == g.nav->res24[0]);
    CHECK(g.nav->pos_u[0] == g.nav->pos_u[1]);
    // The residue is zero whenever the segment is not a moving diagonal: run a short diagonal out
    // (one L_d step at 2^26: 6074000896 * 500 / (2^26 * 100) = 452.5 ticks) and check the reset.
    Case e;
    e.nodes = {sim::NavNode{sim::NavSegKind::Move, 3, 0, sim::kNavDiagStepQ32, 1U << 26}};
    sim::Figure h = committed_ship(e);
    sim::nav_step(h, sim::NavStepEnv{&facts, 100}, planner, 100, ev);
    CHECK(h.nav->res24[0] == (100ULL * (1ULL << 26) * 100 * 11863283ULL) % (1ULL << 24));
    sim::nav_step(h, sim::NavStepEnv{&facts, 100}, planner, 1000, ev);
    CHECK_FALSE(h.nav->order.has_value());
    CHECK(h.nav->res24[0] == 0);
    CHECK(h.nav->res24[1] == 0);
}
TEST_CASE(
    "stepper: the planner is asked at a segment end, with deferral, failure, arrival and commit") {
    const sim::NavFacts facts = facts_with(kTurn);
    struct Scripted : sim::NavPlanner {
        std::vector<sim::NavPlan> script;
        std::size_t next = 0;
        int calls = 0;
        sim::NavPlan plan(const sim::Figure&) override {
            ++calls;
            return next < script.size() ? script[next++] : sim::NavPlan{};
        }
    };
    const auto waiting_ship = [&](std::uint64_t order, std::uint64_t ticket) {
        sim::Figure f;
        f.pos = sim::FixedPos{100, 200};
        f.dir16 = 5;
        f.nav = sim::make_nav_motion(f.pos, 0);
        f.nav->order = sim::NavOrder{order, false, 300, 400};
        f.nav->ticket = ticket;
        return f;
    };
    // The load-time idle segment (remaining 0.01) ends within one 5-tick visit: the planner runs.
    {
        Scripted p;
        p.script = {sim::NavPlan{sim::NavPlanKind::Deferred, {}, {}}};
        sim::Figure f = waiting_ship(5, 9);
        std::vector<sim::NavEvent> ev;
        sim::nav_step(f, sim::NavStepEnv{&facts, 100}, p, 5, ev);
        CHECK(p.calls == 1);
        CHECK(ev.empty());
        REQUIRE(f.nav->order.has_value());
        CHECK_FALSE(f.nav->order->routed);
        CHECK(f.nav->ticket ==
              std::optional<std::uint64_t>(9)); // quota deferral keeps order and ticket
        CHECK(f.nav->seg.kind == sim::NavSegKind::Idle);
        CHECK(f.nav->seg.remaining_x500 == 0); // asked again at the next visit
        sim::nav_step(f, sim::NavStepEnv{&facts, 100}, p, 10, ev);
        CHECK(p.calls == 2); // retried; the script is exhausted: default plan = Failed
        REQUIRE(ev.size() == 1);
        CHECK(ev[0].kind == sim::NavEventKind::RouteFailed);
        CHECK(ev[0].order_id == 5);
        CHECK(ev[0].reason == sim::NavFailReason::Unreachable);
        CHECK_FALSE(f.nav->order.has_value());
        CHECK_FALSE(f.nav->ticket.has_value());
        CHECK(f.nav->seg.kind == sim::NavSegKind::Idle);
        // the plan failed at the start of the second visit; the visit's 5 ticks then ran on the new
        // idle segment (idle rate 42949672 * 100 per tick)
        CHECK(f.nav->seg.remaining_x500 == (std::uint64_t{1} << 32) * 500 - 5ULL * 42949672 * 100);
        // The idle ship asks nobody afterwards.
        sim::nav_step(f, sim::NavStepEnv{&facts, 100}, p, 600, ev);
        CHECK(p.calls == 2);
    }
    // Budget reason is carried; Arrived (status 2, no runs) consumes the order.
    {
        Scripted p;
        p.script = {sim::NavPlan{sim::NavPlanKind::Failed, sim::NavFailReason::BudgetExceeded, {}}};
        sim::Figure f = waiting_ship(6, 10);
        std::vector<sim::NavEvent> ev;
        sim::nav_step(f, sim::NavStepEnv{&facts, 100}, p, 5, ev);
        REQUIRE(ev.size() == 1);
        CHECK(ev[0].reason == sim::NavFailReason::BudgetExceeded);
        Scripted q;
        q.script = {sim::NavPlan{sim::NavPlanKind::Arrived, {}, {}}};
        sim::Figure g = waiting_ship(7, 11);
        ev.clear();
        sim::nav_step(g, sim::NavStepEnv{&facts, 100}, q, 5, ev);
        REQUIRE(ev.size() == 1);
        CHECK(ev[0] == sim::NavEvent{sim::NavEventKind::Arrived, 7, {}});
        CHECK_FALSE(g.nav->order.has_value());
        CHECK(g.pos.x == 100); // no movement
    }
    // A plan: nodes installed, order routed, ticket consumed, facing snapped to the first movement,
    // no position written, leftover time of the visit carries into the new plan.
    {
        Scripted p;
        sim::NavPlan plan;
        plan.kind = sim::NavPlanKind::Nodes;
        plan.nodes = {sim::NavNode{sim::NavSegKind::Move, 4, 0, 2 * kUnit, kSpeed}};
        p.script = {plan};
        sim::Figure f = waiting_ship(8, 12);
        std::vector<sim::NavEvent> ev;
        sim::nav_step(f, sim::NavStepEnv{&facts, 100}, p, 8, ev); // 5 ticks to the plan, 3 sailed
        CHECK(p.calls == 1);
        REQUIRE(f.nav->order.has_value());
        CHECK(f.nav->order->routed);
        CHECK_FALSE(f.nav->ticket.has_value());
        CHECK(f.dir16 == 8); // 2 * dir8 4, directly
        CHECK(f.nav->seg.kind == sim::NavSegKind::Move);
        // the idle segment (remaining 0.01 * 500 at rate idle * 100) ends after exactly 5 ticks, so
        // the 3 leftover ticks convert idle -> speed: 3 * kSpeed * 100 units along +x
        CHECK(f.nav->pos_u[0] == std::int64_t{100} * sim::kNavPosUnitsPerQ8 + 3LL * kSpeed * 100);
        CHECK(f.nav->pos_u[1] == std::int64_t{200} * sim::kNavPosUnitsPerQ8);
        CHECK((f.state_word & 1U) == 1U);
    }
    // A routed order whose plan only held turn nodes has no movement: arrived, nothing installed.
    {
        Scripted p;
        sim::NavPlan plan;
        plan.kind = sim::NavPlanKind::Nodes;
        plan.nodes = {sim::NavNode{sim::NavSegKind::Turn, 0, 3, kTurn, sim::kNavIdleSpeedQ32}};
        p.script = {plan};
        sim::Figure f = waiting_ship(9, 13);
        std::vector<sim::NavEvent> ev;
        sim::nav_step(f, sim::NavStepEnv{&facts, 100}, p, 5, ev);
        REQUIRE(ev.size() == 1);
        CHECK(ev[0].kind == sim::NavEventKind::Arrived);
        CHECK(f.nav->nodes.empty());
    }
}

TEST_CASE("stepper: stop trims a moving chunk to the remainder past whole units, idles the rest") {
    const sim::NavFacts facts = facts_with(kTurn);
    (void)facts;
    const auto ship_with = [&](sim::NavSegKind kind, std::uint8_t dir8,
                               std::uint64_t remaining_x500) {
        Case c;
        c.nodes = {sim::NavNode{sim::NavSegKind::Move, dir8, 0, 5 * kUnit, kSpeed}};
        sim::Figure f = committed_ship(c);
        f.nav->seg = sim::NavSegment{kind, dir8, 100,
                                     kind == sim::NavSegKind::Move ? kSpeed : sim::kNavIdleSpeedQ32,
                                     remaining_x500};
        f.nav->nodes.push_back(sim::NavNode{sim::NavSegKind::Move, 4, 0, kUnit, kSpeed});
        f.dir16 = static_cast<std::uint8_t>(2 * dir8);
        return f;
    };
    const std::uint64_t unit = (std::uint64_t{1} << 32) * 500;
    // A Move chunk with exactly 1.0 left is trimmed to 0, with 0.75 it keeps 0.75, nodes are
    // cleared.
    {
        sim::Figure f = ship_with(sim::NavSegKind::Move, 4, unit);
        sim::nav_stop_motion(f);
        CHECK(f.nav->seg.kind == sim::NavSegKind::Move);
        CHECK(f.nav->seg.remaining_x500 == 0);
        CHECK(f.nav->nodes.empty());
    }
    {
        sim::Figure f = ship_with(sim::NavSegKind::Move, 4, unit * 3 / 4);
        sim::nav_stop_motion(f);
        CHECK(f.nav->seg.remaining_x500 == unit * 3 / 4);
        CHECK(f.nav->nodes.empty());
    }
    { // diagonal chunks trim by 1.0 as well (selector bit 0 of the even facing) and keep the
      // residue
        sim::Figure f = ship_with(sim::NavSegKind::Move, 3, unit / 2);
        f.nav->res24[0] = f.nav->res24[1] = 12345;
        sim::nav_stop_motion(f);
        CHECK(f.nav->seg.remaining_x500 == unit / 2);
        CHECK(f.nav->res24[0] == 12345);
        sim::Figure g = ship_with(sim::NavSegKind::Move, 3, unit);
        g.nav->res24[0] = g.nav->res24[1] = 12345;
        sim::nav_stop_motion(g);
        CHECK(g.nav->seg.remaining_x500 == 0);
        CHECK(g.nav->res24[0] == 0); // nothing diagonal left to sail: the residue is reset
    }
    // A stepper turn phase is untouched; a planner turn node and a long idle become the short idle.
    {
        sim::Figure f = ship_with(sim::NavSegKind::TurnPhase, 0, 1000);
        sim::nav_stop_motion(f);
        CHECK(f.nav->seg.kind == sim::NavSegKind::TurnPhase);
        CHECK(f.nav->seg.remaining_x500 == 1000);
        CHECK(f.nav->nodes.empty());
        sim::Figure g = ship_with(sim::NavSegKind::Turn, 0, 1000);
        sim::nav_stop_motion(g);
        CHECK(g.nav->seg.kind == sim::NavSegKind::Idle);
        CHECK(g.nav->seg.remaining_x500 == sim::kNavIdleSpeedQ32 * 500ULL);
        CHECK((g.state_word & 1U) == 0);
        sim::Figure h = ship_with(sim::NavSegKind::Idle, 0, unit);
        sim::nav_stop_motion(h);
        CHECK(h.nav->seg.remaining_x500 == sim::kNavIdleSpeedQ32 * 500ULL);
        // an idle with 0.01 or less left is not re-armed (a repeated stop cannot starve a ticket)
        sim::Figure k = ship_with(sim::NavSegKind::Idle, 0, 5000);
        sim::nav_stop_motion(k);
        CHECK(k.nav->seg.remaining_x500 == 5000);
        sim::nav_stop_motion(h);
        CHECK(h.nav->seg.remaining_x500 == sim::kNavIdleSpeedQ32 * 500ULL);
    }
    // After the trim the ship sails the remainder out and then idles (order handled by the caller).
    {
        sim::Figure f = ship_with(sim::NavSegKind::Move, 4, unit * 3 / 4);
        f.nav->order.reset();
        sim::nav_stop_motion(f);
        NoPlanner p;
        std::vector<sim::NavEvent> ev;
        sim::nav_step(f, sim::NavStepEnv{&facts, 100}, p, 60000, ev);
        CHECK(f.nav->seg.kind == sim::NavSegKind::Idle);
        CHECK(ev.empty());
        CHECK(f.nav->pos_u[0] == static_cast<std::int64_t>(unit * 3 / 4) + 0);
    }
}

TEST_CASE("stepper: the speed factor follows the original formula with the stated clamps") {
    const sim::NavRules rules;
    const sim::NavFacts facts{kSpeed, kTurn, 6000, 4, 2, 0, 1280};
    const std::uint32_t cap = 1000 * 256; // capacity_88: 1000 tons of the hand-picked example
    const auto holds = [](std::initializer_list<std::uint64_t> amounts) {
        std::vector<sim::ShipHold> h;
        sim::GoodsId g = 1;
        for (const std::uint64_t a : amounts) {
            h.push_back(sim::ShipHold{g++, a});
        }
        return h;
    };
    // Empty ship: L 100, V = (65 + 35) = 100.
    CHECK(sim::nav_speed_factor(facts, rules, cap, holds({}), 1, std::nullopt) == 100);
    // Four holds each full (n0 = 4): S = 4 * cap, divisor (4 + 2) * cap = 6 cap: loss = trunc(17 *
    // 4 / 6) = 11, L = 89, V = trunc(100 * 89 / 100) = 89.
    CHECK(sim::nav_speed_factor(facts, rules, cap, holds({cap, cap, cap, cap}), 1, std::nullopt) ==
          89);
    // Half of one hold: loss trunc(17 * 0.5 / 6) = 1: L 99, V 99.
    CHECK(sim::nav_speed_factor(facts, rules, cap, holds({cap / 2}), 1, std::nullopt) == 99);
    // Only the first n0 = 4 holds count; a fifth is ignored.
    CHECK(sim::nav_speed_factor(facts, rules, cap, holds({cap, cap, cap, cap, cap}), 1,
                                std::nullopt) == 89);
    // Amounts above the capacity are clamped (S stays within (n0 + n1) cap): V stays >= 83.
    CHECK(sim::nav_speed_factor(facts, rules, cap, holds({~0ULL, ~0ULL, ~0ULL, ~0ULL}), 1,
                                std::nullopt) >= 83);
    // Another Volk number: no cargo term. No capacity, or no holds counts: none either.
    CHECK(sim::nav_speed_factor(facts, rules, cap, holds({cap, cap, cap, cap}), 2, std::nullopt) ==
          100);
    CHECK(sim::nav_speed_factor(facts, rules, 0, holds({cap}), 1, std::nullopt) == 100);
    sim::NavFacts none = facts;
    none.hold_count_a = none.hold_count_b = 0;
    CHECK(sim::nav_speed_factor(none, rules, cap, holds({cap}), 1, std::nullopt) == 100);
    // Rules: floor 1, span 0, penalty 17: V = trunc(1 * 100 / 100) = 1; never 0.
    sim::NavRules low = rules;
    low.health_floor_percent = 1;
    low.health_span_percent = 0;
    CHECK(sim::nav_speed_factor(facts, low, cap, holds({}), 1, std::nullopt) == 1);
    CHECK(sim::nav_speed_factor(facts, low, cap, holds({cap, cap, cap, cap}), 1, std::nullopt) ==
          1);
}

TEST_CASE("stepper: saving and loading the motion state at any visit does not change the voyage") {
    // The persisted NavMotion (plus Figure pos, facing, state) is everything the next visit needs:
    // stop after k visits, round-trip through the archive, continue; the result must be byte-equal
    // to the uninterrupted run. Splits fall in turn phases, diagonal segments with residue, speed
    // changes.
    const std::vector<Case> cases = load_cases();
    for (const char* wanted : {"turn_phases_135", "diag_all_four", "speed_change_fraction",
                               "planner_reversal_8_nodes", "mixed_directions", "stamp_wrap"}) {
        const auto it = std::find_if(cases.begin(), cases.end(),
                                     [&](const Case& c) { return c.name == wanted; });
        REQUIRE(it != cases.end());
        const Case& c = *it;
        INFO(c.name);
        const sim::NavFacts facts = facts_with(c.turn);
        const sim::NavStepEnv env{&facts, c.v};
        NoPlanner planner;
        const std::size_t n = c.visits.size();
        // reference run
        sim::Figure ref = committed_ship(c);
        std::vector<std::vector<std::uint8_t>> snapshots;
        std::uint64_t cum = 0;
        for (std::size_t i = 0; i < n; ++i) {
            cum += c.visits[i];
            std::vector<sim::NavEvent> ev;
            sim::nav_step(ref, env, planner, now_of(c.last, cum), ev);
            snapshots.push_back(motion_bytes(ref));
        }
        // interrupted runs at a spread of split points
        for (std::size_t split = 1; split < n; split += std::max<std::size_t>(1, n / 23)) {
            sim::Figure f = committed_ship(c);
            cum = 0;
            for (std::size_t i = 0; i < split; ++i) {
                cum += c.visits[i];
                std::vector<sim::NavEvent> ev;
                sim::nav_step(f, env, planner, now_of(c.last, cum), ev);
            }
            // save and load: the motion record through the archive, the rest by value
            sim::WriteArchive w;
            sim::NavMotion m = *f.nav;
            fields(w, m);
            sim::ReadArchive r(w.data());
            sim::NavMotion loaded;
            fields(r, loaded);
            REQUIRE_FALSE(r.failed());
            sim::Figure g = f;
            g.nav = loaded;
            for (std::size_t i = split; i < n; ++i) {
                cum += c.visits[i];
                std::vector<sim::NavEvent> ev;
                sim::nav_step(g, env, planner, now_of(c.last, cum), ev);
                CHECK(motion_bytes(g) == snapshots[i]);
            }
        }
    }
}

TEST_CASE("stepper: turn segments run at V = 100, a planner turn node ends on its exact tick") {
    // A planner turn node lasts turn_step * 500 / (0.01f * 100) = 55 ticks for 0.11 whatever the
    // ship's speed factor (the original's turn speed is 0.01 and the factor is read for movement
    // only). V = 65 is passed; the Move that follows starts at the 55th tick, not the 56th.
    const sim::NavFacts facts = facts_with(kTurn);
    NoPlanner planner;
    Case c;
    c.nodes = {sim::NavNode{sim::NavSegKind::Turn, 0, 7, kTurn, sim::kNavIdleSpeedQ32},
               sim::NavNode{sim::NavSegKind::Move, 4, 0, 3 * kUnit, kSpeed}};
    sim::Figure f = committed_ship(c);
    f.dir16 = 8;
    std::vector<sim::NavEvent> ev;
    const auto at = [&](std::uint64_t tick) {
        sim::nav_step(f, sim::NavStepEnv{&facts, 65}, planner, static_cast<std::uint16_t>(tick),
                      ev);
    };
    at(1); // the zero-length first segment ends, the Turn node starts: facing 7, not moving
    CHECK(f.nav->seg.kind == sim::NavSegKind::Turn);
    CHECK(f.dir16 == 7);
    CHECK((f.state_word & 1U) == 0);
    at(54);
    CHECK(f.nav->seg.kind == sim::NavSegKind::Turn);
    at(55); // 54 ticks into the node's 55: still turning
    CHECK(f.nav->seg.kind == sim::NavSegKind::Turn);
    at(56); // the node started at tick 1 and lasts exactly 55 ticks: it ends at tick 56
    CHECK(f.nav->seg.kind == sim::NavSegKind::Move);
    CHECK(f.nav->seg.factor_percent == 65); // movement reads V
    CHECK((f.state_word & 1U) == 1U);
    CHECK(f.dir16 ==
          8); // from 7 the heading E (8) is one sixteenth away: no phase, the facing snaps
}

TEST_CASE("stepper: the stepper's own turn phase sets the moving bit") {
    const sim::NavFacts facts = facts_with(kTurn);
    NoPlanner planner;
    Case c;
    c.nodes = {sim::NavNode{sim::NavSegKind::Move, 0, 0, 2 * kUnit, kSpeed}};
    sim::Figure f = committed_ship(c);
    f.dir16 = 8;      // facing E, the node wants W: a reversal
    f.state_word = 0; // not moving
    f.nav->seg.remaining_x500 = 0;
    std::vector<sim::NavEvent> ev;
    // (committed_ship set the facing to the first movement; the reversal needs it back)
    sim::nav_step(f, sim::NavStepEnv{&facts, 100}, planner, 1, ev);
    CHECK(f.nav->seg.kind == sim::NavSegKind::TurnPhase);
    CHECK((f.state_word & 1U) == 1U);
    CHECK(f.dir16 == 7);
}

TEST_CASE(
    "stepper: a deferral drops the visit's leftover and the planner sees the ship where it is") {
    const sim::NavFacts facts = facts_with(kTurn);
    struct Recording : sim::NavPlanner {
        std::vector<sim::NavPlan> script;
        std::size_t next = 0;
        int calls = 0;
        sim::FixedPos seen;
        sim::NavPlan plan(const sim::Figure& ship) override {
            ++calls;
            seen = ship.pos;
            return next < script.size() ? script[next++] : sim::NavPlan{};
        }
    };
    {
        // an 8-tick visit: the idle segment ends at tick 5 (0.01 at speed 0.01), 3 ticks are left;
        // the planner defers: it is asked once, nothing moves, the 3 ticks are not carried.
        sim::Figure f;
        f.pos = sim::FixedPos{100, 200};
        f.nav = sim::make_nav_motion(f.pos, 0);
        f.nav->order = sim::NavOrder{5, false, 300, 400};
        f.nav->ticket = 9;
        Recording p;
        p.script = {sim::NavPlan{sim::NavPlanKind::Deferred, {}, {}}};
        std::vector<sim::NavEvent> ev;
        sim::nav_step(f, sim::NavStepEnv{&facts, 100}, p, 8, ev);
        CHECK(p.calls == 1);
        CHECK(f.nav->last == 8);
        CHECK(f.nav->seg.remaining_x500 == 0);
        CHECK(f.pos.x == 100);
    }
    {
        // A replaced order on a ship that is still sailing its trimmed remainder: the planner is
        // asked at the segment end and must see the position already advanced by that remainder
        // (half a tile east = 128 Q24.8 units), not the position of the visit's start.
        sim::Figure f;
        f.pos = sim::FixedPos{25600, 200};
        f.dir16 = 8;
        f.nav = sim::make_nav_motion(f.pos, 0);
        f.nav->order = sim::NavOrder{6, false, 300, 400};
        f.nav->ticket = 10;
        f.nav->seg = sim::NavSegment{sim::NavSegKind::Move, 4, 100, kSpeed,
                                     (std::uint64_t{1} << 31) * 500}; // 0.5 tile left
        f.state_word |= 1;
        Recording p;
        p.script = {sim::NavPlan{sim::NavPlanKind::Deferred, {}, {}}};
        std::vector<sim::NavEvent> ev;
        sim::nav_step(f, sim::NavStepEnv{&facts, 100}, p, 60000, ev);
        CHECK(p.calls == 1);
        CHECK(p.seen.x == 25600 + 128);
        CHECK(p.seen.y == 200);
    }
}

TEST_CASE("stepper: the speed factor truncates the cargo loss and the product") {
    sim::NavRules rules;
    const std::uint32_t cap = 1000 * 256;
    const sim::NavFacts facts{kSpeed, kTurn, 6000, 4, 2, 0, 1280};
    // 17 * S / (6 cap) = 1.6 exactly at S = 144565.33: S = 144565 gives 1.59999: loss trunc 1 (a
    // rounding implementation would give 2), L 99, V 99.
    CHECK(sim::nav_speed_factor(facts, rules, cap, std::vector<sim::ShipHold>{{1, 144565}}, 1,
                                std::nullopt) == 99);
    // V = trunc(98 * 75 / 100) = trunc(73.5) = 73 (a rounding implementation gives 74): span 33,
    // floor 65, penalty 25, four holds of four, all full: loss = 25 -> L 75.
    rules.health_span_percent = 33;
    rules.health_floor_percent = 65;
    rules.cargo_penalty_percent = 25;
    const sim::NavFacts four{kSpeed, kTurn, 6000, 4, 0, 0, 1280};
    const std::vector<sim::ShipHold> full = {{1, cap}, {2, cap}, {3, cap}, {4, cap}};
    CHECK(sim::nav_speed_factor(four, rules, cap, full, 1, std::nullopt) == 73);
}
