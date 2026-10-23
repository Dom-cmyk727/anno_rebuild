// World-level time of World::advance (docs/sim/state.md §5.3, §5.7, §5.9, §5.10, §14.2,
// §14.3, stage S12): the timed-event dispatch (step 2), the figure registry's phases B, C
// and D (step 3), the script timers (step 5), the monopoly refresh (step 6) and the 7.5
// minute event (step 7), the serial-phase merge rule C2 for event handlers and figure
// seams, the AI seam's commands, and the golden G4. Every expected number is derived from
// the spec's formulas (and the AnnoWelt.dll addresses it cites) by hand, never from the
// code under test; the PRNG values come from the generated table (msvc_rand_sequence),
// which sim_test.cpp holds to the DLL's own.
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/driver.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/rng.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/timers.hpp>
#include <anno/sim/world.hpp>

#include "golden.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;

namespace {

syn::SyntheticTypes types() {
    auto t = syn::make_synthetic_types();
    REQUIRE_MESSAGE(t.ok(), (t.ok() ? std::string() : t.error()));
    return std::move(t).value();
}

sim::World make(const sim::ScenarioSpec& spec, const syn::SyntheticTypes& t, sim::Seams seams = {},
                sim::Rules rules = {}, sim::SimConfig cfg = {}) {
    auto w = sim::World::create(spec, t.registry, std::move(cfg), rules, std::move(seams));
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    return std::move(w).value();
}

constexpr sim::BuildingId bid(std::uint32_t island, std::uint32_t serial) {
    return sim::BuildingId{(std::uint64_t{island} << 32) | serial};
}

// The serials of synthetic::g4_scenario: island 0, city 0.
constexpr std::uint32_t kWeaver = 1;
constexpr std::uint32_t kFarm = 2;

sim::Building& building(sim::World& w, std::uint32_t island, std::uint32_t serial) {
    sim::Building* b =
        sim::find_building_mut(*sim::WorldTestAccess::islands(w)[island], bid(island, serial));
    REQUIRE(b != nullptr);
    return *b;
}

/// The player in slot `id` (it must be there).
const sim::Player& player(const sim::World& w, std::uint32_t id) {
    const sim::Player* p = w.players().find(sim::PlayerId{id});
    REQUIRE(p != nullptr);
    return *p;
}

/// The figure `id` (it must be there).
const sim::Figure& figure(const sim::World& w, sim::FigureId id) {
    const sim::Figure* f = sim::find_figure(w.figures(), id);
    REQUIRE(f != nullptr);
    return *f;
}

template <class T> T& payload(sim::World& w, std::uint32_t island, std::uint32_t serial) {
    T* p = std::get_if<T>(&building(w, island, serial).payload);
    REQUIRE(p != nullptr);
    return *p;
}

/// The G4 world with its two buildings switched off (no production, no carrier of their
/// own): the registry tests place their figures by hand.
sim::World quiet_g4(const syn::SyntheticTypes& t, sim::Seams seams = {}, sim::Rules rules = {}) {
    sim::World w = make(syn::g4_scenario(t), t, std::move(seams), rules);
    payload<sim::Workshop>(w, 0, kWeaver).switched_on = false;
    payload<sim::Farm>(w, 0, kFarm).switched_on = false;
    return w;
}

/// Puts a figure with the next id into the registry (the slot by the list policy), with
/// `home`'s counter `counter` raised for it, and returns its id.
sim::FigureId put_figure(sim::World& w, sim::BuildingId home, std::uint32_t counter = 0,
                         std::optional<std::uint16_t> expiry_stamp = std::nullopt,
                         bool deferred = false, std::uint32_t born = 0) {
    sim::IdCounters& ids = sim::WorldTestAccess::ids(w);
    sim::FigureRegistry& reg = sim::WorldTestAccess::figures(w);
    sim::Figure f;
    f.id = sim::FigureId{ids.next_figure};
    f.kind = 1;
    f.owner = sim::PlayerId{0};
    f.home = home;
    f.home_counter = counter;
    f.born = born;
    f.expiry_stamp = expiry_stamp;
    f.marked_for_delete = deferred;
    REQUIRE(sim::add_figure(reg, f, w.config(), w.rules()) != sim::kNoSlot);
    ++ids.next_figure;
    if (expiry_stamp.has_value()) {
        reg.expiry.push_back(f.id);
    }
    if (deferred) {
        reg.deferred.push_back(f.id);
    }
    auto& islands = sim::WorldTestAccess::islands(w);
    const std::uint32_t slot = sim::island_of(home).v;
    if (home.valid() && slot < islands.size() && islands[slot].has_value()) {
        if (sim::Building* b = sim::find_building_mut(*islands[slot], home)) {
            if (std::uint32_t* c = sim::figure_counter(b->payload, counter)) {
                ++*c;
            }
        }
    }
    return f.id;
}

/// Queues `e` due at tick `due` with the world's next sequence number.
void queue(sim::World& w, std::uint64_t due, const sim::TimedEvent& e) {
    REQUIRE(sim::WorldTestAccess::events(w).enqueue(sim::WorldTestAccess::ids(w), 0, due, e));
}

sim::TimedEvent tile_event(std::uint32_t value) {
    sim::TimedEvent e;
    e.kind = sim::EventKind::TileCleanup;
    e.value = value;
    e.tile = sim::TileXY{static_cast<std::int32_t>(value), 7};
    return e;
}

sim::TimedEvent hit_event(std::uint32_t value) {
    sim::TimedEvent e;
    e.kind = sim::EventKind::DelayedHit;
    e.subtype = 2;
    e.value = value;
    e.target = sim::FigureId{value};
    e.attacker_kind = 2;
    e.attacker = sim::FigureId{value + 1};
    return e;
}

/// The two handler seams: each records "cleanup <value>" or "hit <value>" and, for a
/// value listed in `on`, queues those events (merged right after the handler, C2).
struct EventScript {
    std::vector<std::string> seen;
    std::vector<std::uint64_t> ticks;
    std::map<std::uint32_t, std::vector<sim::EnqueueEvent>> on;
    std::map<std::uint32_t, std::vector<sim::SimEvent>> ui;
};

void handle(EventScript& s, const char* what, const sim::TickContext& ctx, sim::SerialWorld& sw,
            const sim::TimedEvent& e) {
    s.seen.push_back(std::string(what) + " " + std::to_string(e.value));
    s.ticks.push_back(ctx.tick);
    if (const auto it = s.on.find(e.value); it != s.on.end()) {
        sw.fx.events.insert(sw.fx.events.end(), it->second.begin(), it->second.end());
    }
    if (const auto it = s.ui.find(e.value); it != s.ui.end()) {
        sw.fx.ui.insert(sw.fx.ui.end(), it->second.begin(), it->second.end());
    }
}

void install_handlers(sim::Seams& seams, EventScript* s) {
    seams.tile_cleanup.name = "test_cleanup";
    seams.tile_cleanup.ctx = s;
    seams.tile_cleanup.fn = [](void* c, const sim::TickContext& ctx, sim::SerialWorld& sw,
                               const sim::TimedEvent& e) {
        handle(*static_cast<EventScript*>(c), "cleanup", ctx, sw, e);
    };
    seams.delayed_hit.name = "test_hit";
    seams.delayed_hit.ctx = s;
    seams.delayed_hit.fn = [](void* c, const sim::TickContext& ctx, sim::SerialWorld& sw,
                              const sim::TimedEvent& e) {
        handle(*static_cast<EventScript*>(c), "hit", ctx, sw, e);
    };
}

sim::EnqueueEvent later(std::uint64_t delay, const sim::TimedEvent& e) {
    sim::EnqueueEvent q;
    q.delay = delay;
    q.e = e;
    return q;
}

std::vector<std::string> strings(std::initializer_list<const char*> l) {
    return std::vector<std::string>(l.begin(), l.end());
}

/// The draws of one site in the last advance.
std::uint32_t draws_at(const sim::World& w, sim::DrawSite site) {
    for (const sim::DrawRecord& r : w.last_output().draws) {
        if (r.site == site) {
            return r.count;
        }
    }
    return 0;
}

/// The generated PRNG table (sim_test.cpp holds it to the one in AnnoWelt.dll) and the
/// value the k-th draw after cursor c returns: next() = T[(cursor + 1) mod len].
const std::vector<std::uint16_t>& table() {
    static const std::vector<std::uint16_t> t = sim::msvc_rand_sequence(2048, 1);
    return t;
}

std::uint16_t draw_after(std::uint32_t cursor, std::uint32_t k) {
    return table()[(cursor + k) % table().size()];
}

/// The first cursor whose next two draws satisfy `ok(r1, r2)`.
template <class Ok> std::uint32_t cursor_where(Ok ok) {
    for (std::uint32_t c = 0; c < table().size(); ++c) {
        if (ok(draw_after(c, 1), draw_after(c, 2))) {
            return c;
        }
    }
    FAIL("no cursor in the table satisfies the condition");
    return 0;
}

/// A DrawSource that returns scripted values and records the sites asked.
struct ScriptDraw final : sim::DrawSource {
    std::vector<std::uint16_t> values;
    std::vector<sim::DrawSite> sites;
    std::uint16_t next(sim::DrawSite site) override {
        sites.push_back(site);
        REQUIRE(sites.size() <= values.size());
        return values[sites.size() - 1];
    }
};

std::uint64_t hash_events(const std::vector<sim::SimEvent>& events) {
    sim::StateHasher h;
    for (const sim::SimEvent& e : events) {
        h.u8(static_cast<std::uint8_t>(e.kind));
        h.u64(e.tick);
        h.u32(e.player.v);
        h.u64(e.building.v);
        h.u64(e.figure.v);
        h.u32(e.code);
        h.u32(static_cast<std::uint32_t>(e.x));
        h.u32(static_cast<std::uint32_t>(e.y));
    }
    return h.value();
}

std::filesystem::path g4_path() {
    return std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "g4.log";
}

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in),
                                     std::istreambuf_iterator<char>());
}

void check_golden_line(const std::string& name, std::uint64_t value) {
    std::string line;
    const std::string problem = anno::test::check_golden(name, "S12", value, line);
    if (anno::test::update_mode()) {
        MESSAGE("golden line: " << line);
    }
    CHECK_MESSAGE(problem.empty(), problem);
}

} // namespace

// ------------------------------------------------------------- step 2: timed events

TEST_CASE("FIFO: timed events due at the same tick run in the order they were queued") {
    // D 0x100adef0 inserts a node after every node with due <= its own: the key (due,
    // seq) serves equal due ticks first come, first served; an earlier due tick goes
    // first whatever its sequence number. Each kind reaches its own handler (D 0x100add90:
    // kind 0 -> 0x100adae0, kind 1 -> 0x100adb80).
    const syn::SyntheticTypes t = types();
    EventScript s;
    sim::Seams seams;
    install_handlers(seams, &s);
    sim::World w = make(syn::empty_scenario(), t, seams);
    queue(w, 20, hit_event(1));  // (20, 1)
    queue(w, 20, tile_event(2)); // (20, 2)
    queue(w, 20, hit_event(3));  // (20, 3)
    queue(w, 10, tile_event(4)); // (10, 4)
    w.advance(30);               // tick 15: only (10, 4) is due
    CHECK(s.seen == strings({"cleanup 4"}));
    CHECK(w.last_output().events_dispatched == 1);
    w.advance(10); // tick 20: due <= now
    CHECK(s.seen == strings({"cleanup 4", "hit 1", "cleanup 2", "hit 3"}));
    CHECK(s.ticks == std::vector<std::uint64_t>{15, 20, 20, 20});
    CHECK(w.last_output().events_dispatched == 3);
    CHECK(w.events().q.empty());
    CHECK(w.check_invariants() == "");
}

TEST_CASE("saved-next in advance: what a handler queues is served only behind the saved node") {
    // D 0x100add90 saves the next node before it calls the handler (pop_due, D3, OQ-038):
    // an event queued behind the saved node is served in the same advance, one queued in
    // front of it or after the last node waits for the next advance.
    const syn::SyntheticTypes t = types();
    auto run = [&](bool drain_new, std::uint64_t b_due) {
        EventScript s;
        s.on[1] = {later(0, hit_event(10))};  // A queues X, due now
        s.on[10] = {later(0, hit_event(20))}; // X queues Y, due now
        sim::Seams seams;
        install_handlers(seams, &s);
        sim::Rules rules;
        rules.events_drain_new = drain_new;
        sim::World w = make(syn::empty_scenario(), t, seams, rules);
        queue(w, 10, tile_event(1));    // A
        queue(w, b_due, tile_event(2)); // B
        w.advance(40);                  // tick 20
        std::vector<std::string> first = s.seen;
        w.advance(2); // tick 21
        CHECK(w.check_invariants() == "");
        return std::pair{first, s.seen};
    };
    SUBCASE("behind the saved node") {
        // A (10, 1) and B (10, 2) are due. A queues X at (20, 3): behind B, so after B the
        // saved node is X and X is served; X is then the last node and queues Y at (20, 4),
        // which this advance does not see.
        const auto [first, all] = run(false, 10);
        CHECK(first == strings({"cleanup 1", "cleanup 2", "hit 10"}));
        CHECK(all == strings({"cleanup 1", "cleanup 2", "hit 10", "hit 20"}));
        // Rules::events_drain_new reads the queue's head again after every event.
        const auto [drained, _] = run(true, 10);
        CHECK(drained == strings({"cleanup 1", "cleanup 2", "hit 10", "hit 20"}));
    }
    SUBCASE("in front of the saved node") {
        // B is due at 1000: the saved node after A is B. X (20, 3) lands in front of it,
        // the loop goes on at B, which is not due: X waits for the next advance although
        // it is due now. At tick 21 X is served; the Y it queues at (21, 4) lands in front
        // of B again and waits once more.
        const auto [first, all] = run(false, 1000);
        CHECK(first == strings({"cleanup 1"}));
        CHECK(all == strings({"cleanup 1", "hit 10"}));
        const auto [drained, _] = run(true, 1000);
        CHECK(drained == strings({"cleanup 1", "hit 10", "hit 20"}));
    }
}

TEST_CASE("handler effects are merged right after each handler (C2)") {
    // Each handler sees the effects of the handlers before it: its UI events reach the
    // advance's output in dispatch order, its money is booked before the next handler.
    const syn::SyntheticTypes t = types();
    struct Seen {
        std::vector<std::int64_t> money;
    } seen;
    sim::Seams seams;
    seams.delayed_hit.name = "test_pay";
    seams.delayed_hit.ctx = &seen;
    seams.delayed_hit.fn = [](void* c, const sim::TickContext& ctx, sim::SerialWorld& sw,
                              const sim::TimedEvent& e) {
        static_cast<void>(ctx.draw.next(sim::DrawSite::CityRandomEvents)); // telemetry
        const sim::Player* p = sw.players().find(sim::PlayerId{0});
        static_cast<Seen*>(c)->money.push_back(p != nullptr ? p->money : 0);
        sim::MoneyDelta m;
        m.player = sim::PlayerId{0};
        m.delta = -static_cast<std::int64_t>(e.value);
        sw.fx.money.push_back(m);
    };
    sim::World w = make(syn::g4_scenario(t), t, seams);
    queue(w, 5, hit_event(100));
    queue(w, 5, hit_event(20));
    queue(w, 5, hit_event(3));
    w.advance(10);
    CHECK(seen.money == std::vector<std::int64_t>{10000, 9900, 9880});
    CHECK(player(w, 0).money == 9877);
    // A handler's draws are told per site in the advance's output.
    CHECK(draws_at(w, sim::DrawSite::CityRandomEvents) == 3);
}

TEST_CASE("an event the island phase queues due now is dispatched in the same advance") {
    // §5.3: the islands' Effects are merged (step 1) before the timed events are popped
    // (step 2), so an event queued with delay 0 by an island tick is served by the same
    // advance; one with delay 1 waits for the next tick.
    const syn::SyntheticTypes t = types();
    struct Once {
        EventScript s;
        int calls = 0;
    } once;
    sim::Seams seams;
    install_handlers(seams, &once.s);
    seams.city_pass[0].name = "test_queue_once";
    seams.city_pass[0].ctx = &once;
    seams.city_pass[0].fn = [](void* c, const sim::TickContext&, sim::Island&, sim::City&,
                               sim::Effects& fx) {
        auto& o = *static_cast<Once*>(c);
        if (o.calls++ == 0) {
            fx.events.push_back(later(0, hit_event(1)));
            fx.events.push_back(later(1, hit_event(2)));
        }
    };
    sim::World w = make(syn::g4_scenario(t), t, seams);
    w.advance(100); // tick 50
    CHECK(once.s.seen == strings({"hit 1"}));
    CHECK(once.s.ticks == std::vector<std::uint64_t>{50});
    w.advance(2); // tick 51
    CHECK(once.s.seen == strings({"hit 1", "hit 2"}));
}

TEST_CASE("kind-1 events saved: both kinds survive save and load and fire alike") {
    // F7: the original's loader drops pending kind-1 events (D 0x100ad900..0x100ad9ec,
    // 0x100ada30); here both kinds are saved and loaded with every payload field.
    const syn::SyntheticTypes t = types();
    EventScript s1;
    sim::Seams seams1;
    install_handlers(seams1, &s1);
    sim::World w = make(syn::g4_scenario(t), t, seams1);
    sim::TimedEvent tile_hit = hit_event(7);
    tile_hit.subtype = sim::kTileSubtype;
    tile_hit.tile = sim::TileXY{-3, 9};
    queue(w, 100, hit_event(5));
    queue(w, 50, tile_hit);
    queue(w, 70, tile_event(6));
    w.advance(20); // tick 10, nothing due
    const std::vector<std::uint8_t> bytes = w.save();
    EventScript s2;
    sim::Seams seams2;
    install_handlers(seams2, &s2);
    auto loaded = sim::World::load(bytes, t.registry, seams2);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    sim::World v = std::move(loaded).value();
    CHECK(v.state_hash() == w.state_hash());
    REQUIRE(v.events().q.size() == 3);
    auto a = w.events().q.begin();
    auto b = v.events().q.begin();
    for (; a != w.events().q.end(); ++a, ++b) {
        CHECK(a->first == b->first);
        CHECK(a->second.kind == b->second.kind);
        CHECK(a->second.subtype == b->second.subtype);
        CHECK(a->second.value == b->second.value);
        CHECK(a->second.tile == b->second.tile);
        CHECK(a->second.target == b->second.target);
        CHECK(a->second.attacker_kind == b->second.attacker_kind);
        CHECK(a->second.attacker == b->second.attacker);
    }
    for (int i = 0; i < 10; ++i) {
        w.advance(20);
        v.advance(20);
    }
    CHECK(s1.seen == strings({"hit 7", "cleanup 6", "hit 5"}));
    CHECK(s2.seen == s1.seen);
    CHECK(s2.ticks == std::vector<std::uint64_t>{50, 70, 100});
    CHECK(v.state_hash() == w.state_hash());
}

TEST_CASE("events flush after figures: a delay-0 hit of the figure phase waits one advance") {
    // D4 (OQ-039): the event pass runs before the figure phase, so a delayed hit a figure
    // queues with delay 0 is served by the next advance; Rules::events_flush_after_figures
    // adds a second pass after the figures.
    const syn::SyntheticTypes t = types();
    auto run = [&](bool flush) {
        EventScript s;
        sim::Seams seams;
        install_handlers(seams, &s);
        seams.figure_tick.name = "test_shooter";
        seams.figure_tick.fn = [](void*, const sim::TickContext&, sim::SerialWorld& sw,
                                  sim::Figure& f) {
            sw.fx.events.push_back(later(0, hit_event(static_cast<std::uint32_t>(f.id.v))));
        };
        sim::Rules rules;
        rules.events_flush_after_figures = flush;
        sim::World w = quiet_g4(t, seams, rules);
        put_figure(w, bid(0, kWeaver));
        w.advance(100); // tick 50
        std::vector<std::uint64_t> first = s.ticks;
        const std::uint64_t dispatched = w.last_output().events_dispatched;
        w.advance(100); // tick 100
        return std::tuple{first, s.ticks, dispatched};
    };
    const auto [plain_first, plain_all, plain_n] = run(false);
    CHECK(plain_first.empty());
    CHECK(plain_all == std::vector<std::uint64_t>{100});
    CHECK(plain_n == 0);
    const auto [flush_first, flush_all, flush_n] = run(true);
    CHECK(flush_first == std::vector<std::uint64_t>{50});
    CHECK(flush_all == std::vector<std::uint64_t>{50, 100});
    CHECK(flush_n == 1); // the second pass is counted in the advance's output
}

TEST_CASE("a dispatch stopped at the cap is reported by check_invariants until the next advance") {
    // F24: one pass serves at most Rules::events_max_dispatch events; the rest stays queued
    // and the stop is reported.
    const syn::SyntheticTypes t = types();
    sim::Rules rules;
    rules.events_max_dispatch = 1;
    sim::World w = make(syn::empty_scenario(), t, {}, rules);
    queue(w, 5, tile_event(1));
    queue(w, 5, tile_event(2));
    w.advance(10);
    CHECK(w.last_output().events_capped);
    CHECK(w.last_output().events_dispatched == 1);
    CHECK(w.events().q.size() == 1);
    CHECK(w.check_invariants().find("events_max_dispatch") != std::string::npos);
    w.advance(10);
    CHECK_FALSE(w.last_output().events_capped);
    CHECK(w.events().q.empty());
    CHECK(w.check_invariants() == "");
}

// ------------------------------------------------------------- step 3: figures

TEST_CASE("registry count without +1: phase B visits clamp(d n / 50, 1, n) slots") {
    // D 0x1009aa4c..0x1009aa77: count = d·n/50 clamped to 1 .. n, no +1 (the building
    // lists and the city ring add 1); the cursor moves on by the count modulo n.
    const syn::SyntheticTypes t = types();
    std::vector<std::uint64_t> visited;
    sim::Seams seams;
    seams.figure_tick.name = "test_visits";
    seams.figure_tick.ctx = &visited;
    seams.figure_tick.fn = [](void* c, const sim::TickContext&, sim::SerialWorld&, sim::Figure& f) {
        static_cast<std::vector<std::uint64_t>*>(c)->push_back(f.id.v);
    };
    sim::World w = make(syn::empty_scenario(), t, seams);
    // Two figures: the list grows from 1 to 10 slots (A7, §5.2); slots 0 and 1.
    const sim::FigureId a = put_figure(w, sim::BuildingId{});
    const sim::FigureId b = put_figure(w, sim::BuildingId{});
    REQUIRE(w.figures().list.slots.size() == 10);
    REQUIRE(w.figures().by_id.at(a) == 0);
    REQUIRE(w.figures().by_id.at(b) == 1);
    w.advance(40); // t = 20, d = 20: 20 * 10 / 50 = 4 visits (with +1 it would be 5)
    CHECK(visited == std::vector<std::uint64_t>{a.v, b.v});
    CHECK(w.figures().list.cursor == 4);
    CHECK(w.figures().stamp == 20);
    w.advance(2); // t = 21, d = 1: 10 / 50 = 0, raised to 1: slot 4
    CHECK(w.figures().list.cursor == 5);
    w.advance(120); // t = 81, d = 60: 12, cut to 10: slots 5..9 and 0..4
    CHECK(visited == std::vector<std::uint64_t>{a.v, b.v, a.v, b.v});
    CHECK(w.figures().list.cursor == 5);
    CHECK(w.figures().stamp == 81);
    // figure_b_div is configuration: /25 doubles the count.
    sim::SimConfig cfg;
    cfg.figure_b_div = 25;
    sim::World v = make(syn::empty_scenario(), t, seams, {}, cfg);
    put_figure(v, sim::BuildingId{});
    put_figure(v, sim::BuildingId{});
    v.advance(40); // d = 20: 20 * 10 / 25 = 8
    CHECK(v.figures().list.cursor == 8);
}

TEST_CASE("expiry: phase C ends figures 7500 ticks old from the front and stops at a young one") {
    // D 0x1009aacc..0x1009ab31: from the front of the expiry list, a figure whose u16
    // difference t - stamp is at least 7500 (0x1d4c, `jb` at 0x1009ab07) is destroyed
    // (0x10097520: its slot emptied, its home counter released); the first younger one
    // ends the scan, even when an older one stands behind it.
    const syn::SyntheticTypes t = types();
    sim::World w = quiet_g4(t);
    const sim::FigureId old1 = put_figure(w, bid(0, kWeaver), 0, std::uint16_t{0});
    const sim::FigureId young = put_figure(w, bid(0, kFarm), 0, std::uint16_t{100});
    const sim::FigureId old2 = put_figure(w, bid(0, kWeaver), 0, std::uint16_t{0});
    REQUIRE(payload<sim::Workshop>(w, 0, kWeaver).carriers_out == 2);
    REQUIRE(payload<sim::Farm>(w, 0, kFarm).workers_out == 1);
    REQUIRE(w.check_invariants() == "");
    w.advance(14998); // tick 7499: 7499 < 7500 for all three
    CHECK(w.figures().expiry == std::vector<sim::FigureId>{old1, young, old2});
    w.advance(2); // tick 7500: old1 ends; young (7400) stops the scan before old2
    CHECK(w.figures().expiry == std::vector<sim::FigureId>{young, old2});
    CHECK(sim::find_figure(w.figures(), old1) == nullptr);
    CHECK_FALSE(w.figures().list.slots[0].has_value());
    CHECK(payload<sim::Workshop>(w, 0, kWeaver).carriers_out == 1);
    CHECK(w.check_invariants() == "");
    w.advance(200); // tick 7600: young (7500) and old2 end
    CHECK(w.figures().expiry.empty());
    CHECK(w.figures().by_id.empty());
    CHECK(payload<sim::Workshop>(w, 0, kWeaver).carriers_out == 0);
    CHECK(payload<sim::Farm>(w, 0, kFarm).workers_out == 0);
    CHECK(w.check_invariants() == "");
}

TEST_CASE("expiry compares a 16-bit difference across the wrap of the tick") {
    const syn::SyntheticTypes t = types();
    sim::World w = quiet_g4(t);
    sim::WorldTestAccess::clock(w).ms = 2 * 65000; // tick 65000
    const sim::FigureId f = put_figure(w, bid(0, kWeaver), 0, std::uint16_t{65000});
    w.advance(2 * 7499); // tick 72499: u16(72499 - 65000) = 7499
    CHECK(sim::find_figure(w.figures(), f) != nullptr);
    w.advance(2); // tick 72500, low 16 bits 6964: u16(6964 - 65000) = 7500
    CHECK(sim::find_figure(w.figures(), f) == nullptr);
    CHECK(w.check_invariants() == "");
    // A stamp taken after the first wrap: at tick 70000 the stamp is u16(70000) = 4464, so
    // a 32-bit difference would read 65586 at tick 70050 and end the figure at once.
    sim::World v = quiet_g4(t);
    sim::WorldTestAccess::clock(v).ms = 2 * 70000;
    const sim::FigureId g = put_figure(v, bid(0, kWeaver), 0, std::uint16_t{4464});
    v.advance(100); // tick 70050: u16 difference 50
    CHECK(sim::find_figure(v.figures(), g) != nullptr);
    v.advance(2 * 7449); // tick 77499: 7499
    CHECK(sim::find_figure(v.figures(), g) != nullptr);
    v.advance(2); // tick 77500: 7500
    CHECK(sim::find_figure(v.figures(), g) == nullptr);
}

TEST_CASE("counter release: phase D frees the slot, keeps hint and length, releases the counter") {
    // D 0x1009ab33..0x1009ab7c: every figure on the deferred list is deleted and the list
    // emptied. A figure on both lists ends in phase C and is released once; a figure
    // releases the counter it holds (a small farm's animal counter 2 here).
    const syn::SyntheticTypes t = types();
    sim::ScenarioSpec spec = syn::g4_scenario(t);
    {
        sim::SmallFarm sf = sim::make_small_farm(t.registry->find(t.small_farm)->prod);
        sf.line_on[0] = false; // both lines off: no step, no draw, no figure of its own
        sf.line_on[1] = false;
        sim::ScenarioSpec::BuildingSpec b;
        b.type = t.small_farm;
        b.x = 115;
        b.y = 115;
        b.payload = sim::Payload{sf};
        spec.islands[0].cities[0].buildings.push_back(b);
    }
    sim::World w = make(spec, t);
    payload<sim::Workshop>(w, 0, kWeaver).switched_on = false;
    payload<sim::Farm>(w, 0, kFarm).switched_on = false;
    constexpr std::uint32_t kSmallFarm = 3;
    const sim::FigureId a = put_figure(w, bid(0, kWeaver));
    const sim::FigureId b = put_figure(w, bid(0, kWeaver), 0, std::nullopt, true);
    // Stamp u16(50 - 7500): 7500 old at tick 50, so phase C ends it before phase D.
    const sim::FigureId both =
        put_figure(w, bid(0, kWeaver), 0, static_cast<std::uint16_t>(50 - 7500), true);
    const sim::FigureId animal = put_figure(w, bid(0, kSmallFarm), 2, std::nullopt, true);
    const sim::FigureId orphan = put_figure(w, bid(0, 77), 0, std::nullopt, true);
    REQUIRE(payload<sim::Workshop>(w, 0, kWeaver).carriers_out == 3);
    REQUIRE(payload<sim::SmallFarm>(w, 0, kSmallFarm).figures_out ==
            std::vector<std::uint32_t>{0, 0, 1, 0, 0});
    const std::int64_t hint = w.figures().list.hint;
    const std::uint64_t next = w.ids().next_figure;
    REQUIRE(w.check_invariants() == "");
    w.advance(100);
    CHECK(sim::find_figure(w.figures(), a) != nullptr);
    CHECK(sim::find_figure(w.figures(), b) == nullptr);
    CHECK(sim::find_figure(w.figures(), both) == nullptr);
    CHECK(sim::find_figure(w.figures(), animal) == nullptr);
    CHECK(sim::find_figure(w.figures(), orphan) == nullptr);
    CHECK(w.figures().deferred.empty());
    CHECK(w.figures().expiry.empty());
    CHECK(w.figures().by_id.size() == 1);
    CHECK(w.figures().list.slots.size() == 10);
    CHECK(w.figures().list.hint == hint);
    CHECK(w.ids().next_figure == next);
    CHECK(payload<sim::Workshop>(w, 0, kWeaver).carriers_out == 1); // 3 - b - both
    CHECK(payload<sim::SmallFarm>(w, 0, kSmallFarm).figures_out ==
          std::vector<std::uint32_t>{0, 0, 0, 0, 0});
    CHECK(w.check_invariants() == "");
    // The freed slots are taken again by the list policy (the scan from hint + 1).
    const sim::FigureId c = put_figure(w, bid(0, kWeaver));
    CHECK(w.figures().by_id.at(c) == static_cast<sim::Slot>(hint + 1));
}

TEST_CASE("interim return: with the rule a figure ends N ticks after its birth") {
    // D12 (OQ-047, a stand-in): without a behaviour seam a figure never returns, so its
    // counter stays raised; with Rules::interim_figure_return_ticks = N, phase B marks a
    // figure whose u32 age t - born is at least N, and phase D ends it in the same tick.
    const syn::SyntheticTypes t = types();
    auto run = [&](std::uint32_t n) {
        sim::Rules rules;
        rules.interim_figure_return_ticks = n;
        sim::World w = make(syn::g4_scenario(t), t, {}, rules);
        std::vector<std::pair<std::uint64_t, std::uint32_t>> spawns; // (id, born)
        std::map<std::uint64_t, std::uint64_t> gone;                 // id -> first tick gone
        for (int i = 0; i < 200; ++i) {
            w.advance(100);
            for (const sim::SimEvent& e : w.last_output().events) {
                if (e.kind == sim::SimEventKind::FigureSpawned) {
                    spawns.emplace_back(e.figure.v, static_cast<std::uint32_t>(e.tick));
                }
            }
            for (const auto& [id, born] : spawns) {
                if (!gone.contains(id) &&
                    sim::find_figure(w.figures(), sim::FigureId{id}) == nullptr) {
                    gone[id] = w.clock().tick();
                }
            }
            REQUIRE(w.check_invariants() == "");
        }
        return std::pair{spawns, gone};
    };
    // Off (the default): the weaver's carrier and the farm's worker never come back.
    const auto [kept, never] = run(0);
    CHECK(kept.size() == 2);
    CHECK(never.empty());
    // N = 1000: every figure ends at the first advance (50 ticks each; phase B visits all
    // ten slots, clamp(50 · 10 / 50, 1, 10)) whose tick is born + 1000 or later, and the
    // building sends the next one.
    const auto [spawns, gone] = run(1000);
    CHECK(spawns.size() > 4);
    for (const auto& [id, born] : spawns) {
        if (gone.contains(id)) {
            CHECK(gone.at(id) >= std::uint64_t{born} + 1000);
            CHECK(gone.at(id) < std::uint64_t{born} + 1000 + 50);
        }
    }
}

TEST_CASE("a figure seam writes its own figure and the registry lists follow its flags") {
    // The seam protocol (C2, §4.11): a behaviour seam may write the figure it is called
    // for; a figure that gains a expiry_stamp joins the expiry list, one that marks
    // itself for deletion joins the deferred list (and ends in phase D of the same tick),
    // and the figure's id stays the registry's.
    const syn::SyntheticTypes t = types();
    sim::Seams seams;
    seams.figure_tick.name = "test_self";
    seams.figure_tick.fn = [](void*, const sim::TickContext& ctx, sim::SerialWorld&,
                              sim::Figure& f) {
        if (f.kind == 1) {
            f.expiry_stamp = static_cast<std::uint16_t>(ctx.t);
            f.kind = 2;
        } else if (f.kind == 3) {
            f.marked_for_delete = true;
        } else if (f.kind == 4) {
            f.id = sim::FigureId{999}; // not the seam's to change
        } else if (f.kind == 5) {
            f.expiry_stamp.reset(); // leaves the expiry list
        } else if (f.kind == 6) {
            f.marked_for_delete = false; // leaves the deferred list
        }
    };
    sim::World w = quiet_g4(t, seams);
    const sim::FigureId stamped = put_figure(w, bid(0, kWeaver));
    const sim::FigureId leaving = put_figure(w, bid(0, kFarm));
    const sim::FigureId renamed = put_figure(w, bid(0, kWeaver));
    const sim::FigureId unstamped = put_figure(w, bid(0, kWeaver), 0, std::uint16_t{0});
    const sim::FigureId kept = put_figure(w, bid(0, kWeaver), 0, std::nullopt, true);
    sim::WorldTestAccess::figures(w).list.slots[1]->kind = 3;
    sim::WorldTestAccess::figures(w).list.slots[2]->kind = 4;
    sim::WorldTestAccess::figures(w).list.slots[3]->kind = 5;
    sim::WorldTestAccess::figures(w).list.slots[4]->kind = 6;
    REQUIRE(w.figures().expiry == std::vector<sim::FigureId>{unstamped});
    REQUIRE(w.figures().deferred == std::vector<sim::FigureId>{kept});
    w.advance(100); // tick 50
    CHECK(w.figures().expiry == std::vector<sim::FigureId>{stamped});
    CHECK(w.figures().deferred.empty());
    CHECK_FALSE(figure(w, unstamped).expiry_stamp.has_value());
    CHECK_FALSE(figure(w, kept).marked_for_delete);
    CHECK(figure(w, stamped).expiry_stamp == std::uint16_t{50});
    CHECK(sim::find_figure(w.figures(), leaving) == nullptr);
    CHECK(payload<sim::Farm>(w, 0, kFarm).workers_out == 0);
    CHECK(sim::find_figure(w.figures(), renamed) != nullptr);
    CHECK(w.check_invariants() == "");
    w.advance(2 * 7450); // tick 7500: 7450 < 7500, still there
    CHECK(sim::find_figure(w.figures(), stamped) != nullptr);
    w.advance(100); // tick 7550: 7500, ends
    CHECK(sim::find_figure(w.figures(), stamped) == nullptr);
    CHECK(sim::find_figure(w.figures(), unstamped) != nullptr);     // off the expiry list
    CHECK(payload<sim::Workshop>(w, 0, kWeaver).carriers_out == 3); // renamed, unstamped, kept
    CHECK(w.check_invariants() == "");
}

TEST_CASE("a figure spawned during phase B waits for the next pass") {
    // n is read once before the loop (0x1009aa3a) and the slot looked up again in every
    // iteration (0x1009aa93): a spawn merged after a figure's call grows the list, but this
    // pass still visits the n slots it started with; the new figure is visited next time.
    const syn::SyntheticTypes t = types();
    struct Spawner {
        std::vector<std::uint64_t> visited;
        bool spawned = false;
    } sp;
    sim::Seams seams;
    seams.figure_tick.name = "test_spawner";
    seams.figure_tick.ctx = &sp;
    seams.figure_tick.fn = [](void* c, const sim::TickContext&, sim::SerialWorld& sw,
                              sim::Figure& f) {
        auto& s = *static_cast<Spawner*>(c);
        s.visited.push_back(f.id.v);
        if (f.id.v == 1 && !s.spawned) {
            s.spawned = true;
            sim::SpawnRequest r;
            r.figure_kind = 9;
            r.owner = sim::PlayerId{0};
            sw.fx.spawns.push_back(r);
        }
    };
    sim::World w = make(syn::empty_scenario(), t, seams);
    for (int i = 0; i < 10; ++i) {
        put_figure(w, sim::BuildingId{}); // slots 0 .. 9: the list of 10 is full
    }
    REQUIRE(w.figures().list.slots.size() == 10);
    w.advance(100);                             // d = 50: clamp(50 * 10 / 50, 1, 10) = 10 visits
    CHECK(w.figures().list.slots.size() == 12); // grown 10 -> 12 by the spawn (§5.2)
    CHECK(sp.visited == std::vector<std::uint64_t>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    CHECK(w.figures().list.cursor == 0);
    sp.visited.clear();
    w.advance(100); // d = 50: clamp(50 * 12 / 50, 1, 12) = 12: the new figure (11) too
    CHECK(sp.visited == std::vector<std::uint64_t>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11});
    CHECK(w.check_invariants() == "");
}

TEST_CASE("interim return reads a u32 age and is the default body only") {
    // D12: the age is t - born in u32 (state.md §5.9); a behaviour seam replaces the
    // default body, so with a seam installed the rule does nothing.
    const syn::SyntheticTypes t = types();
    sim::Rules rules;
    rules.interim_figure_return_ticks = 66000;
    sim::World w = quiet_g4(t, {}, rules);
    const sim::FigureId f = put_figure(w, bid(0, kWeaver));
    w.advance(2 * 65950); // tick 65950: age 65950
    CHECK(sim::find_figure(w.figures(), f) != nullptr);
    w.advance(100); // tick 66000: age 66000 (a u16 age would be 464)
    CHECK(sim::find_figure(w.figures(), f) == nullptr);
    CHECK(payload<sim::Workshop>(w, 0, kWeaver).carriers_out == 0);
    sim::Seams seams;
    seams.figure_tick.name = "test_idle";
    seams.figure_tick.fn = [](void*, const sim::TickContext&, sim::SerialWorld&, sim::Figure&) {};
    sim::Rules quick;
    quick.interim_figure_return_ticks = 1000;
    sim::World v = quiet_g4(t, seams, quick);
    const sim::FigureId g = put_figure(v, bid(0, kWeaver));
    for (int i = 0; i < 40; ++i) {
        v.advance(100); // tick 2000
    }
    CHECK(sim::find_figure(v.figures(), g) != nullptr);
}

TEST_CASE("figure phase goods effects merge after each call (C2)") {
    // Each figure's Effects are merged right after its call (C2): the second cart sees the
    // store the first emptied and the stock the first changed, as the original's
    // synchronous carts would; a Reserve from a building that is gone takes 0.
    const syn::SyntheticTypes t = types();
    struct Carts {
        std::uint32_t goods = 0;
        std::vector<std::uint32_t> store_seen, stock_seen;
        int calls = 0;
    } carts;
    sim::Seams seams;
    seams.figure_tick.name = "test_carts";
    seams.figure_tick.ctx = &carts;
    seams.figure_tick.fn = [](void* c, const sim::TickContext& ctx, sim::SerialWorld& sw,
                              sim::Figure& f) {
        auto& k = *static_cast<Carts*>(c);
        ++k.calls;
        const sim::CityRef city{sim::IslandId{0}, sim::CityNo{0}};
        const sim::Building* b = sw.find_building(bid(0, kWeaver));
        const sim::Workshop* weaver =
            b != nullptr ? std::get_if<sim::Workshop>(&b->payload) : nullptr;
        const sim::City* home = sw.city(city);
        if (weaver == nullptr || weaver->out.empty() || home == nullptr ||
            k.goods >= home->stock.stock.size()) {
            return;
        }
        k.store_seen.push_back(weaver->out.front());
        k.stock_seen.push_back(home->stock.stock[k.goods]);
        if (ctx.tick == 50) {
            // First lap: take 600 from the store, put 256 into the stock, and try a
            // building that does not exist.
            sw.fx.reserves.push_back(sim::Reserve{f.id, bid(0, kWeaver), 0, 600});
            sw.fx.reserves.push_back(sim::Reserve{f.id, bid(0, 99), 0, 600});
            sw.fx.goods_moves.push_back(sim::GoodsMove{city, k.goods, 256});
        } else {
            // Second lap: deliver the cargo.
            sw.fx.deliveries.push_back(sim::Deliver{f.id, city});
        }
    };
    sim::World w = quiet_g4(t, seams);
    carts.goods = w.types().find(t.weaver)->output_goods[0];
    payload<sim::Workshop>(w, 0, kWeaver).out[0] = 1000;
    const sim::FigureId a = put_figure(w, bid(0, kWeaver));
    const sim::FigureId b = put_figure(w, bid(0, kWeaver));
    w.advance(100); // tick 50: both carts, a first (slot 0)
    CHECK(carts.store_seen == std::vector<std::uint32_t>{1000, 400});
    CHECK(carts.stock_seen == std::vector<std::uint32_t>{0, 256});
    CHECK(figure(w, a).cargo_88 == 600);
    CHECK(figure(w, b).cargo_88 == 400);
    CHECK(payload<sim::Workshop>(w, 0, kWeaver).out[0] == 0);
    w.advance(100); // tick 100: both deliver, b sees a's delivery
    CHECK(carts.stock_seen == std::vector<std::uint32_t>{0, 256, 512, 1112});
    CHECK(w.islands()[0]->city(sim::CityNo{0})->stock.stock[carts.goods] == 1512);
    CHECK(figure(w, a).cargo_88 == 0);
    CHECK(figure(w, b).cargo_88 == 0);
    CHECK(carts.calls == 4);
    CHECK(w.check_invariants() == "");
}

// ------------------------------------------------------------- step 4: the AI

TEST_CASE("AI seam commands applied in advance and not logged") {
    // A15, §8.2: the AI's commands are applied inside the advance, at once and in order,
    // and nobody logs them: a recorded session holds only advances and checkpoints, and a
    // replay needs the same seam (its name is in the header) to reproduce them.
    const syn::SyntheticTypes t = types();
    struct Ai {
        int calls = 0;
        sim::GoodsId goods = 0;
    };
    auto seams_for = [](Ai* ai) {
        sim::Seams s;
        s.ai.name = "test_ai_line_off";
        s.ai.ctx = ai;
        s.ai.fn = [](void* c, const sim::TickContext&, const sim::SerialWorld&,
                     std::vector<sim::Command>& out) {
            auto& a = *static_cast<Ai*>(c);
            if (++a.calls == 3) {
                out.push_back(
                    sim::Command{sim::PlayerId{0},
                                 sim::SetProductionLineCmd{sim::TileXY{110, 110}, a.goods, false}});
            }
        };
        return s;
    };
    Ai ai;
    sim::World w = make(syn::g4_scenario(t), t, seams_for(&ai));
    ai.goods = w.types().find(t.weaver)->output_goods[0];
    auto started = sim::Recorder::start(w);
    REQUIRE_MESSAGE(started.ok(), (started.ok() ? std::string() : started.error()));
    sim::Recorder rec = std::move(started).value();
    rec.checkpoint();
    for (int i = 1; i <= 10; ++i) {
        rec.advance(100);
        rec.checkpoint();
        CHECK(payload<sim::Workshop>(w, 0, kWeaver).switched_on == (i < 3));
    }
    CHECK(ai.calls == 10);
    for (const sim::ReplayEntry& e : rec.log().entries) {
        CHECK(e.kind != sim::ReplayEntry::Kind::Commands);
    }
    // The log replays with the same seam installed (a fresh one: the AI runs again and
    // issues its command again inside the third advance).
    Ai again;
    again.goods = ai.goods;
    auto fresh = sim::World::load(rec.log().initial_save, t.registry, seams_for(&again));
    REQUIRE_MESSAGE(fresh.ok(), (fresh.ok() ? std::string() : fresh.error()));
    sim::World replayed = std::move(fresh).value();
    auto outcome = sim::replay(replayed, rec.log());
    REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
    CHECK(outcome.value().reproduced());
    CHECK(outcome.value().checkpoints_matched == 11);
    CHECK(again.calls == 10);
    // Without the seam the save is refused: the seam names are part of the state.
    auto without = sim::World::load(rec.log().initial_save, t.registry, sim::Seams{});
    REQUIRE_FALSE(without.ok());
    CHECK(without.error().find("seam names") != std::string::npos);
}

// ------------------------------------------------------------- steps 5 to 7

TEST_CASE("timer clamp: timer 0 wraps on an overshoot by default and stops at 0 under the rule") {
    // D 0x1009cd30: dt = t - last_tick (u32); running timers k > 0 count up by dt
    // (wrapping); the running timer 0 counts down by dt in u32 and stops only when the
    // result is exactly 0 (0x1009cd51..0x1009cd59 `sub`/`jne`): an overshoot wraps and the
    // timer runs on. That is the default (owner decision on OQ-133); Rules::timer0_clamp
    // (F5) stops it at 0 instead. TimerExpired is posted only when the timer stops.
    constexpr std::uint64_t k2to32 = std::uint64_t{1} << 32;
    sim::ScriptTimers tm = sim::make_script_timers(sim::SimConfig{});
    REQUIRE(tm.active.size() == 5);
    tm.active = {1, 1, 0, 1, 1};
    tm.counter = {100, 0xFFFFFFFEU, 9, 5, 0};
    CHECK_FALSE(sim::timers_tick(tm, 60)); // dt 60
    CHECK(tm.counter == std::vector<std::uint32_t>{40, 58, 9, 65, 60});
    CHECK(tm.last_tick == 60);
    SUBCASE("the original: an overshoot wraps and the timer runs on") {
        CHECK_FALSE(sim::timers_tick(tm, 160)); // dt 100 > 40: 40 - 100 in u32
        CHECK(tm.counter[0] == k2to32 - 60);
        CHECK(tm.active == std::vector<std::uint8_t>{1, 1, 0, 1, 1});
        CHECK(tm.counter == std::vector<std::uint32_t>{static_cast<std::uint32_t>(k2to32 - 60), 158,
                                                       9, 165, 160});
        // An exact landing stops it (the original's own case).
        tm.counter[0] = 30;
        CHECK(sim::timers_tick(tm, 190));
        CHECK(tm.counter[0] == 0);
        CHECK(tm.active[0] == 0);
        CHECK_FALSE(sim::timers_tick(tm, 200)); // a stopped timer stays as it is
        CHECK(tm.counter[0] == 0);
    }
    SUBCASE("Rules::timer0_clamp: an overshoot stops the timer at 0") {
        CHECK(sim::timers_tick(tm, 160, true)); // dt 100 > 40
        CHECK(tm.counter == std::vector<std::uint32_t>{0, 158, 9, 165, 160});
        CHECK(tm.active == std::vector<std::uint8_t>{0, 1, 0, 1, 1});
        CHECK_FALSE(sim::timers_tick(tm, 170, true));
        CHECK(tm.counter[0] == 0);
        tm.active[0] = 1; // an exact landing stops it as well
        tm.counter[0] = 30;
        CHECK(sim::timers_tick(tm, 200, true));
        CHECK(tm.active[0] == 0);
    }
    SUBCASE("dt is a u32 difference across the wrap of t") {
        tm.last_tick = 0xFFFFFFF0U;
        tm.counter[1] = 0;
        CHECK_FALSE(sim::timers_tick(tm, 0x10));
        CHECK(tm.counter[1] == 0x20);
    }
    // Through advance: steps of 50 ticks over a counter of 100.
    const syn::SyntheticTypes t = types();
    auto run = [&](bool clamp, std::uint32_t counter) {
        sim::Rules rules;
        rules.timer0_clamp = clamp;
        sim::World w = make(syn::empty_scenario(), t, {}, rules);
        sim::WorldTestAccess::timers(w).active[0] = 1;
        sim::WorldTestAccess::timers(w).counter[0] = counter;
        w.advance(198); // tick 99
        const std::uint32_t left = w.timers().counter[0];
        w.advance(100); // tick 149
        return std::tuple{left, std::move(w)};
    };
    {
        // The default with an overshoot: 1 left at tick 99, then 1 - 50 wraps; no event.
        auto [left, w] = run(false, 100);
        CHECK(left == 1);
        CHECK(w.timers().counter[0] == k2to32 - 49);
        CHECK(w.timers().active[0] == 1);
        CHECK(w.last_output().events.empty());
        CHECK(w.drain_events().empty());
    }
    {
        // The default with an exact landing: 99 + 50 = 149 ticks.
        auto [left, w] = run(false, 149);
        CHECK(left == 50);
        CHECK(w.timers().counter[0] == 0);
        CHECK(w.timers().active[0] == 0);
        REQUIRE(w.last_output().events.size() == 1);
        CHECK(w.last_output().events[0].kind == sim::SimEventKind::TimerExpired);
        CHECK(w.last_output().events[0].tick == 149);
    }
    {
        // The rule: the overshoot of 49 stops the timer at tick 149 with a TimerExpired.
        auto [left, w] = run(true, 100);
        CHECK(left == 1);
        CHECK(w.timers().counter[0] == 0);
        CHECK(w.timers().active[0] == 0);
        CHECK(w.timers().last_tick == 149);
        REQUIRE(w.last_output().events.size() == 1);
        CHECK(w.last_output().events[0].kind == sim::SimEventKind::TimerExpired);
        CHECK(w.last_output().events[0].code == 0);
        CHECK(w.last_output().events[0].tick == 149);
        const std::vector<sim::SimEvent> host = w.drain_events();
        REQUIRE(host.size() == 1);
        CHECK(host[0].kind == sim::SimEventKind::TimerExpired);
    }
}

TEST_CASE("monopoly owner is the one city of a Volk 1 player on the island") {
    // D 0x1009d210: cities in number order; a city qualifies when its owner's Volk is 1;
    // a second qualifying city, also of the same player, means "none".
    const syn::SyntheticTypes t = types();
    sim::World w = make(syn::g4_scenario(t), t);
    const sim::PlayerTable& p = w.players();
    CHECK(sim::monopoly_owner(*w.islands()[0], p) == sim::PlayerId{0});
    CHECK(sim::monopoly_owner(*w.islands()[2], p) == std::nullopt);     // players 0 and 1
    CHECK(sim::monopoly_owner(*w.islands()[4], p) == sim::PlayerId{1}); // 1, not 2 (Volk 2)
    sim::World same = w;
    sim::WorldTestAccess::islands(same)[2]->cities[1]->owner = sim::PlayerId{0};
    CHECK(sim::monopoly_owner(*same.islands()[2], same.players()) == std::nullopt);
    sim::World none = w;
    sim::WorldTestAccess::players(none).slots[0]->volk = 3;
    CHECK(sim::monopoly_owner(*none.islands()[0], none.players()) == std::nullopt);
}

TEST_CASE("monopoly once and the rule: the faithful refresh runs on the first advance only") {
    // D 0x1009d2a0 (D5, OQ-040): the gate lets the body through when the accumulator is 0
    // or at least 30 000 ms, and only the body adds the step; a first step below 30 s
    // therefore runs the body once and never again. Rules::monopoly_every_30s runs it every
    // 30 s of game time instead.
    //
    // The G4 islands, by hand from the climate masks (D 0x1009d338..0x1009d3aa): island 0,
    // climate 2, mask 0xf949 (bits 0 3 6 8 11 12-15), owner player 0; island 2, climate 1,
    // mask 0xf009 (bits 0 3 12-15), no owner (two Volk-1 cities); island 4, climate 0, mask
    // 0x18000 (bits 15 16), owner player 1. So slot s = 0, 3, 12..15 is cleared at island 2
    // (which ends the scan), 6, 8 and 11 go to player 0, 16 to player 1 (game second 0 +
    // the offset 1000), the rest stays empty.
    const syn::SyntheticTypes t = types();
    auto expect_g4_entries = [](const sim::World& w, std::uint32_t time = 1000) {
        const std::vector<sim::MonopolyEntry>& e = w.monopoly().entries;
        REQUIRE(e.size() == 28);
        for (std::uint32_t s = 0; s < 28; ++s) {
            INFO("entry " << s);
            if (s == 6 || s == 8 || s == 11) {
                CHECK(e[s].player == sim::PlayerId{0});
                CHECK(e[s].time_s == time);
            } else if (s == 16) {
                CHECK(e[s].player == sim::PlayerId{1});
                CHECK(e[s].time_s == time);
            } else {
                CHECK_FALSE(e[s].player.has_value());
                CHECK(e[s].time_s == 0);
            }
        }
    };
    SUBCASE("faithful") {
        sim::World w = make(syn::g4_scenario(t), t);
        w.advance(100);
        CHECK(w.monopoly().acc == 100);
        expect_g4_entries(w);
        // Player 2 becomes Volk 1: island 4 would lose its owner, but the body never runs
        // again (acc stays 100).
        sim::WorldTestAccess::players(w).slots[2]->volk = 1;
        for (int i = 0; i < 400; ++i) {
            w.advance(100);
        }
        CHECK(w.monopoly().acc == 100);
        expect_g4_entries(w);
    }
    SUBCASE("every 30 s under the rule") {
        sim::Rules rules;
        rules.monopoly_every_30s = true;
        sim::World w = make(syn::g4_scenario(t), t, {}, rules);
        w.advance(100);
        CHECK(w.monopoly().acc == 100);
        for (const sim::MonopolyEntry& e : w.monopoly().entries) {
            CHECK_FALSE(e.player.has_value()); // the first refresh is due at 30 s
        }
        for (int i = 1; i < 300; ++i) {
            w.advance(100);
        }
        CHECK(w.monopoly().acc == 0);    // 30 000 reached and taken off
        expect_g4_entries(w, 30 + 1000); // game second 30 (tick 15 000) + the offset
        sim::WorldTestAccess::players(w).slots[2]->volk = 1;
        for (int i = 0; i < 300; ++i) {
            w.advance(100);
        }
        CHECK_FALSE(w.monopoly().entries[16].player.has_value());
        CHECK(w.monopoly().entries[6].player == sim::PlayerId{0});
        // An unchanged owner keeps the time of its first entry (0x1009d3bf skips it).
        CHECK(w.monopoly().entries[6].time_s == 1030);
    }
    SUBCASE("the rule keeps the remainder of the accumulator") {
        // 70 ms steps: 429 of them reach 30 030, the refresh runs and 30 is kept.
        sim::Rules rules;
        rules.monopoly_every_30s = true;
        sim::World w = make(syn::g4_scenario(t), t, {}, rules);
        for (int i = 0; i < 428; ++i) {
            w.advance(70);
        }
        CHECK(w.monopoly().acc == 29960);
        CHECK_FALSE(w.monopoly().entries[6].player.has_value());
        w.advance(70);
        CHECK(w.monopoly().acc == 30);
        CHECK(w.monopoly().entries[6].player == sim::PlayerId{0});
    }
    SUBCASE("only the raw slots are scanned") {
        // monopoly_raw_slots bounds the slot loop (s = 0 .. 19 in the original): with 10,
        // slots 11 and 16 stay empty; a mask bit at or above the bound is never read, so
        // entries 20 .. 27 (the second loop, O-24) are left alone.
        sim::SimConfig cfg;
        cfg.monopoly_raw_slots = 10;
        sim::World w = make(syn::g4_scenario(t), t, {}, {}, cfg);
        w.advance(100);
        CHECK(w.monopoly().entries[6].player == sim::PlayerId{0});
        CHECK(w.monopoly().entries[8].player == sim::PlayerId{0});
        CHECK_FALSE(w.monopoly().entries[11].player.has_value());
        CHECK_FALSE(w.monopoly().entries[16].player.has_value());
        sim::SimConfig wide;
        wide.monopoly_climate_mask[2] |= 1U << 20; // island 0's climate
        sim::World v = make(syn::g4_scenario(t), t, {}, {}, wide);
        v.advance(100);
        CHECK_FALSE(v.monopoly().entries[20].player.has_value());
    }
    SUBCASE("a first step of 0 ms or of 30 s or more lets the body run again") {
        // advance(0) adds nothing: the accumulator stays 0 and the next call runs the body
        // too. A first step of 30 000 ms leaves it at 30 000, so every later call runs it.
        sim::World w = make(syn::g4_scenario(t), t);
        w.advance(0);
        CHECK(w.monopoly().acc == 0);
        expect_g4_entries(w);
        sim::WorldTestAccess::players(w).slots[2]->volk = 1;
        w.advance(0);
        CHECK_FALSE(w.monopoly().entries[16].player.has_value());
        sim::World v = make(syn::g4_scenario(t), t);
        v.advance(30000);
        CHECK(v.monopoly().acc == 30000);
        sim::WorldTestAccess::players(v).slots[2]->volk = 1;
        v.advance(10);
        CHECK(v.monopoly().acc == 30010);
        CHECK_FALSE(v.monopoly().entries[16].player.has_value());
    }
}

TEST_CASE("event 7.5 min draws: one draw in the base game, two in the add-on on a hit") {
    // D 0x100ae140: acc += dt; below 450 000 nothing; acc = 0; property 6 bit 0x80 ends it
    // without a draw; draw #1 % 10; the base game (property 14 bit 2 clear) ends after it;
    // draw #2 % table length picks the island slot, a null slot ends it.
    const syn::SyntheticTypes t = types();
    const sim::World g4 = make(syn::g4_scenario(t), t); // table of 5: slots 0, 2, 4
    const sim::SimConfig cfg;
    const sim::Rules rules;
    sim::WorldSettings addon;
    addon.prop14 = 3;
    sim::WorldSettings base = addon;
    base.prop14 = 1;
    auto fire = [&](std::uint32_t acc, std::uint32_t dt, const sim::WorldSettings& s,
                    std::vector<std::uint16_t> values, const sim::SimConfig& c) {
        ScriptDraw d;
        d.values = std::move(values);
        std::uint32_t a = acc;
        const std::optional<sim::IslandId> hit = sim::event750(a, dt, c, rules, s, g4.islands(), d);
        return std::tuple{hit, d.sites, a};
    };
    using Sites = std::vector<sim::DrawSite>;
    const Sites gate{sim::DrawSite::Event750Gate};
    const Sites both{sim::DrawSite::Event750Gate, sim::DrawSite::Event750Island};
    {
        const auto [hit, sites, acc] = fire(0, 449999, addon, {}, cfg);
        CHECK_FALSE(hit.has_value());
        CHECK(sites.empty());
        CHECK(acc == 449999);
    }
    {
        const auto [hit, sites, acc] = fire(449999, 1, addon, {20, 7}, cfg); // 7 % 5 = 2
        CHECK(hit == sim::IslandId{2});
        CHECK(sites == both);
        CHECK(acc == 0);
    }
    {
        const auto [hit, sites, acc] = fire(449999, 1, addon, {21}, cfg);
        CHECK_FALSE(hit.has_value());
        CHECK(sites == gate);
    }
    {
        const auto [hit, sites, acc] = fire(449999, 1, base, {20}, cfg); // the draw is used
        CHECK_FALSE(hit.has_value());
        CHECK(sites == gate);
    }
    {
        sim::WorldSettings off = addon;
        off.prop6 = 0x80;
        const auto [hit, sites, acc] = fire(449999, 1, off, {}, cfg);
        CHECK_FALSE(hit.has_value());
        CHECK(sites.empty());
        CHECK(acc == 0); // reset before the property test (0x100ae16e)
    }
    {
        const auto [hit, sites, acc] = fire(449999, 1, addon, {30, 8}, cfg); // 8 % 5 = 3: null
        CHECK_FALSE(hit.has_value());
        CHECK(sites == both);
    }
    {
        sim::SimConfig c;
        c.event750_ms = 1000;
        c.event750_modulus = 3;
        const auto [hit, sites, acc] = fire(999, 1, addon, {9, 4}, c); // 4 % 5 = 4
        CHECK(hit == sim::IslandId{4});
        CHECK(sites == both);
    }
    // Through advance: draw counts in the output and the UI event 0x15a at the centre of
    // the island hit (D 0x100ae1d6..0x100ae240: x0 + w / 2, y0 + h / 2).
    const std::uint32_t c = cursor_where(
        [](std::uint16_t r1, std::uint16_t r2) { return r1 % 10 == 0 && r2 % 5 == 4; });
    // Island 4 moved to (301, 330) and made 15 x 9, so x and y, w and h and the rounding
    // of an odd width all show.
    sim::ScenarioSpec spec = syn::g4_scenario(t);
    REQUIRE(spec.islands[2].slot == sim::IslandId{4});
    spec.islands[2].x0 = 301;
    spec.islands[2].y0 = 330;
    spec.islands[2].w = 15;
    spec.islands[2].h = 9;
    sim::World w = make(spec, t);
    sim::WorldTestAccess::rng(w).set_cursor(c);
    sim::WorldTestAccess::event750_acc(w) = 449900;
    w.advance(100); // tick 50: no other site draws this early
    CHECK(draws_at(w, sim::DrawSite::Event750Gate) == 1);
    CHECK(draws_at(w, sim::DrawSite::Event750Island) == 1);
    CHECK(w.rng().cursor() == c + 2);
    REQUIRE(w.last_output().events.size() == 1);
    const sim::SimEvent& e = w.last_output().events[0];
    CHECK(e.kind == sim::SimEventKind::Event15a);
    CHECK(e.code == 0x15a);
    CHECK(e.tick == 50);
    CHECK(e.x == 301 + 7); // x0 + w / 2 (15 / 2 = 7)
    CHECK(e.y == 330 + 4); // y0 + h / 2 (9 / 2 = 4)
    CHECK(w.event750_acc() == 0);
}

TEST_CASE("accumulator discard vs keep: the 7.5 min event drops its overshoot by default") {
    // D10 (OQ-045), D 0x100ae16e: acc = 0 after a firing, the overshoot is lost, so the
    // schedule depends on the step size; Rules::event750_keep_overshoot keeps it.
    const syn::SyntheticTypes t = types();
    const sim::World g4 = make(syn::g4_scenario(t), t);
    const sim::SimConfig cfg;
    sim::Rules keep;
    keep.event750_keep_overshoot = true;
    ScriptDraw d;
    d.values = {1, 1};
    std::uint32_t acc = 449990;
    static_cast<void>(sim::event750(acc, 100, cfg, sim::Rules{}, g4.settings(), g4.islands(), d));
    CHECK(acc == 0);
    acc = 449990;
    static_cast<void>(sim::event750(acc, 100, cfg, keep, g4.settings(), g4.islands(), d));
    CHECK(acc == 90);
    // Through advance with steps of 70 ms: the gate fires at 450 030 ms (6 429 steps).
    // Faithful: the accumulator restarts at 0 and fires again after 6 429 more steps (at
    // 900 060 ms). Kept: it restarts at 30 and needs 6 429 more steps as well (30 + 450 030),
    // keeping 60. Over 15 000 steps (1 050 000 ms) both fire twice; the kept accumulator
    // ends 60 ms ahead.
    auto run = [&](const sim::Rules& rules) {
        sim::World w = make(syn::empty_scenario(), t, {}, rules);
        std::uint32_t gates = 0;
        for (int i = 0; i < 15000; ++i) {
            w.advance(70);
            gates += draws_at(w, sim::DrawSite::Event750Gate);
        }
        return std::pair{gates, w.event750_acc()};
    };
    const auto [faithful_gates, faithful_acc] = run(sim::Rules{});
    const auto [kept_gates, kept_acc] = run(keep);
    CHECK(faithful_gates == 2);
    CHECK(kept_gates == 2);
    // 1 050 000 - 2 * 450 030 = 149 940 (faithful); 1 050 000 - 2 * 450 000 = 150 000.
    CHECK(faithful_acc == 149940);
    CHECK(kept_acc == 150000);
}

TEST_CASE("zero islands: the 7.5 min event ends after its island draw without dividing") {
    // F6 (D 0x100ae1bb divides by the island count): with an empty island table the event
    // makes both draws and ends; nothing else happens.
    const syn::SyntheticTypes t = types();
    ScriptDraw d;
    d.values = {10, 3};
    std::uint32_t acc = 450000;
    const std::vector<std::optional<sim::Island>> none;
    const sim::WorldSettings addon;
    CHECK_FALSE(sim::event750(acc, 0, sim::SimConfig{}, sim::Rules{}, addon, none, d).has_value());
    CHECK(d.sites.size() == 2);
    const std::uint32_t c =
        cursor_where([](std::uint16_t r1, std::uint16_t) { return r1 % 10 == 0; });
    sim::World w = make(syn::empty_scenario(), t);
    REQUIRE(w.islands().empty());
    sim::WorldTestAccess::rng(w).set_cursor(c);
    sim::WorldTestAccess::event750_acc(w) = 449999;
    w.advance(2);
    CHECK(draws_at(w, sim::DrawSite::Event750Gate) == 1);
    CHECK(draws_at(w, sim::DrawSite::Event750Island) == 1);
    CHECK(w.last_output().events.empty());
    CHECK(w.check_invariants() == "");
}

TEST_CASE("advance runs events, figures, players and the AI, then timers, monopoly, 7.5 min") {
    // D 0x100ae040 (§5.3): islands, timed events, figures, players (with the AI), script
    // timers, the monopoly refresh, the 7.5 minute event; each serial call's UI events go to
    // the output in that order.
    const syn::SyntheticTypes t = types();
    struct Order {
        std::vector<std::string> calls;
    } order;
    sim::Seams seams;
    seams.delayed_hit.name = "order_hit";
    seams.delayed_hit.ctx = &order;
    seams.delayed_hit.fn = [](void* c, const sim::TickContext& ctx, sim::SerialWorld& sw,
                              const sim::TimedEvent&) {
        static_cast<Order*>(c)->calls.push_back("event");
        sim::SimEvent e;
        e.kind = sim::SimEventKind::Notice;
        e.tick = ctx.tick;
        e.code = 1;
        sw.fx.ui.push_back(e);
    };
    seams.figure_tick.name = "order_figure";
    seams.figure_tick.ctx = &order;
    seams.figure_tick.fn = [](void* c, const sim::TickContext& ctx, sim::SerialWorld& sw,
                              sim::Figure&) {
        static_cast<Order*>(c)->calls.push_back("figure");
        sim::SimEvent e;
        e.kind = sim::SimEventKind::Notice;
        e.tick = ctx.tick;
        e.code = 2;
        sw.fx.ui.push_back(e);
    };
    seams.player_groups.name = "order_player";
    seams.player_groups.ctx = &order;
    seams.player_groups.fn = [](void* c, const sim::TickContext&, sim::SerialWorld&,
                                sim::Player& p) {
        static_cast<Order*>(c)->calls.push_back("player " + std::to_string(p.index.v));
    };
    seams.ai.name = "order_ai";
    seams.ai.ctx = &order;
    seams.ai.fn = [](void* c, const sim::TickContext&, const sim::SerialWorld&,
                     std::vector<sim::Command>&) { static_cast<Order*>(c)->calls.push_back("ai"); };
    sim::World w = quiet_g4(t, seams);
    queue(w, 1, hit_event(1));
    put_figure(w, bid(0, kWeaver));
    sim::WorldTestAccess::timers(w).active[0] = 1;
    sim::WorldTestAccess::timers(w).counter[0] = 50; // lands exactly at tick 50
    const std::uint32_t c = cursor_where(
        [](std::uint16_t r1, std::uint16_t r2) { return r1 % 10 == 0 && r2 % 5 == 0; });
    sim::WorldTestAccess::rng(w).set_cursor(c);
    sim::WorldTestAccess::event750_acc(w) = 449900;
    w.advance(100); // tick 50; the player table visits all 32 slots (d = 50)
    CHECK(order.calls == strings({"event", "figure", "player 0", "player 1", "player 2", "ai"}));
    std::vector<std::pair<sim::SimEventKind, std::uint32_t>> ui;
    for (const sim::SimEvent& e : w.last_output().events) {
        ui.emplace_back(e.kind, e.code);
    }
    CHECK(ui == std::vector<std::pair<sim::SimEventKind, std::uint32_t>>{
                    {sim::SimEventKind::Notice, 1},
                    {sim::SimEventKind::Notice, 2},
                    {sim::SimEventKind::TimerExpired, 0},
                    {sim::SimEventKind::Event15a, 0x15a}});
    CHECK(w.monopoly().acc == 100);
}

TEST_CASE("advance zero ms still runs the world-level steps") {
    // No early return for 0 (D 0x100ae040): the registry's stamp, the timers' last tick and
    // the monopoly body all run at tick 0; the accumulators add 0.
    const syn::SyntheticTypes t = types();
    sim::World w = make(syn::g4_scenario(t), t);
    sim::WorldTestAccess::timers(w).active[0] = 1; // counter 0: due at once
    sim::WorldTestAccess::figures(w).stamp = 99;
    w.advance(0);
    CHECK(w.figures().stamp == 0);
    CHECK(w.timers().active[0] == 0);
    REQUIRE(w.last_output().events.size() == 1);
    CHECK(w.last_output().events[0].kind == sim::SimEventKind::TimerExpired);
    CHECK(w.monopoly().entries[16].player == sim::PlayerId{1});
    CHECK(w.monopoly().acc == 0);
    CHECK(w.event750_acc() == 0);
}

TEST_CASE("timers and monopoly state is checked, hashed, saved and refused when malformed") {
    const syn::SyntheticTypes t = types();
    sim::World base = make(syn::g4_scenario(t), t);
    base.advance(100);
    sim::WorldTestAccess::timers(base).active[3] = 1;
    sim::WorldTestAccess::timers(base).counter[3] = 77;
    REQUIRE(base.check_invariants() == "");
    // Every new field moves the hash, in its own scope.
    auto scope_hashes = [](const sim::World& w) {
        sim::HashTree tree;
        static_cast<void>(w.state_hash(tree, 0));
        std::vector<std::uint64_t> out;
        for (const sim::HashNode& n : tree.root.children) {
            out.push_back(n.hash);
        }
        return out;
    };
    const std::vector<std::uint64_t> before = scope_hashes(base);
    REQUIRE(before.size() == 9);
    struct Mutation {
        const char* name;
        std::size_t scope;
        void (*apply)(sim::World&);
    };
    const Mutation mutations[] = {
        {"event750_acc", 3, [](sim::World& w) { ++sim::WorldTestAccess::event750_acc(w); }},
        {"monopoly.acc", 3, [](sim::World& w) { ++sim::WorldTestAccess::monopoly(w).acc; }},
        {"faulted", 3, [](sim::World& w) { sim::WorldTestAccess::faulted(w) = true; }},
        {"timers.last_tick", 8, [](sim::World& w) { ++sim::WorldTestAccess::timers(w).last_tick; }},
        {"timers.active", 8, [](sim::World& w) { sim::WorldTestAccess::timers(w).active[4] = 1; }},
        {"timers.counter", 8, [](sim::World& w) { ++sim::WorldTestAccess::timers(w).counter[3]; }},
        {"monopoly time", 8,
         [](sim::World& w) { ++sim::WorldTestAccess::monopoly(w).entries[6].time_s; }},
        {"monopoly player", 8,
         [](sim::World& w) { sim::WorldTestAccess::monopoly(w).entries[6].player.reset(); }},
    };
    for (const Mutation& m : mutations) {
        INFO(m.name);
        sim::World w = base;
        m.apply(w);
        const std::vector<std::uint64_t> after = scope_hashes(w);
        for (std::size_t k = 0; k < after.size(); ++k) {
            CHECK((after[k] != before[k]) == (k == m.scope));
        }
        // And it survives a save and load.
        auto loaded = sim::World::load(w.save(), t.registry);
        REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
        CHECK(loaded.value().state_hash() == w.state_hash());
    }
    // Malformed tables: check_invariants names them and load refuses a save carrying them.
    struct Bad {
        const char* name;
        const char* message;
        void (*apply)(sim::World&);
    };
    const Bad bad[] = {
        {"short active", "timers.active",
         [](sim::World& w) { sim::WorldTestAccess::timers(w).active.pop_back(); }},
        {"long counter", "timers.counter",
         [](sim::World& w) { sim::WorldTestAccess::timers(w).counter.push_back(0); }},
        {"short monopoly", "monopoly.entries",
         [](sim::World& w) { sim::WorldTestAccess::monopoly(w).entries.pop_back(); }},
        {"player outside the table", "monopoly.entries[2].player",
         [](sim::World& w) {
             sim::WorldTestAccess::monopoly(w).entries[2].player = sim::PlayerId{32};
         }},
    };
    for (const Bad& b : bad) {
        INFO(b.name);
        sim::World w = base;
        b.apply(w);
        CHECK(w.check_invariants().find(b.message) != std::string::npos);
        auto loaded = sim::World::load(w.save(), t.registry);
        REQUIRE_FALSE(loaded.ok());
        CHECK(loaded.error().find(b.message) != std::string::npos);
    }
}

// ------------------------------------------------------------- the golden G4

TEST_CASE("G4 players timers events and the 7.5 min event through 600000 ticks equal golden") {
    // G4 (docs/sim/state.md §14.3): synthetic::g4_scenario (seed 508) with the state a
    // scenario cannot carry, 12 000 single-player advances of 100 ms (tick 600 000, 20 game
    // minutes), a checkpoint before the first and after every 600th advance, recorded as
    // fixtures/synthetic/sim/g4.log.
    const syn::SyntheticTypes t = types();
    sim::World w = make(syn::g4_scenario(t), t);
    // Players: 0 has the upkeep sum 330 (330 / 15 = 22 booked per 2 000 ticks, no
    // remainder); 1 starts 2 900 in debt with the sum 600 (40 per booking); 2 (Volk 2) has
    // nothing to pay.
    sim::Player& human = *sim::WorldTestAccess::players(w).slots[0];
    human.upkeep_a = 300;
    human.upkeep_b = 30;
    human.stat_a = 1050;
    sim::WorldTestAccess::players(w).slots[1]->upkeep_a = 600;
    // Timers: timer 0 counts down 100 025 ticks; with the default rules (the original, OQ-133)
    // the step to tick 100 050 overshoots by 25, the counter wraps and the timer runs on;
    // timer 2 counts up from 7, timer 4 is stopped.
    sim::ScriptTimers& tm = sim::WorldTestAccess::timers(w);
    tm.active = {1, 0, 1, 0, 0};
    tm.counter = {100025, 0, 7, 0, 3};
    // Events: two due at tick 1 000 (FIFO), one at 400 000, a delayed hit at 700 000 that is
    // still queued at the end; no handler is installed, so each is consumed.
    queue(w, 1000, tile_event(1));
    queue(w, 1000, hit_event(2));
    queue(w, 400000, tile_event(3));
    sim::TimedEvent late = hit_event(4);
    late.subtype = sim::kTileSubtype;
    late.tile = sim::TileXY{105, 105};
    queue(w, 700000, late);
    // Figures: a carrier of the weaver on the expiry list from tick 0 (the weaver holds its
    // one carrier until phase C ends it at tick 7 500) and a worker of the farm on the
    // deferred list (ended by the first advance).
    const sim::FigureId carrier = put_figure(w, bid(0, kWeaver), 0, std::uint16_t{0});
    const sim::FigureId worker = put_figure(w, bid(0, kFarm), 0, std::nullopt, true);
    REQUIRE(w.check_invariants() == "");

    const std::uint32_t cursor0 = w.rng().cursor();
    auto started = sim::Recorder::start(w);
    REQUIRE_MESSAGE(started.ok(), (started.ok() ? std::string() : started.error()));
    sim::Recorder rec = std::move(started).value();
    rec.checkpoint();
    sim::StateHasher checkpoints;
    std::map<sim::DrawSite, std::uint32_t> draws;
    std::vector<sim::SimEvent> ui;
    std::uint64_t dispatched = 0;
    std::optional<std::uint64_t> carrier_gone, worker_gone, bankrupt_at;
    bool monopoly_once = true;
    for (int i = 1; i <= 12000; ++i) {
        rec.advance(100);
        // The advance's own output (never dropped, unlike the bounded host queue).
        ui.insert(ui.end(), w.last_output().events.begin(), w.last_output().events.end());
        for (const sim::DrawRecord& r : w.last_output().draws) {
            draws[r.site] += r.count;
        }
        dispatched += w.last_output().events_dispatched;
        const std::uint64_t tick = w.clock().tick();
        if (!carrier_gone && sim::find_figure(w.figures(), carrier) == nullptr) {
            carrier_gone = tick;
        }
        if (!worker_gone && sim::find_figure(w.figures(), worker) == nullptr) {
            worker_gone = tick;
        }
        if (!bankrupt_at && player(w, 1).state == sim::PlayerState::Bankrupt) {
            bankrupt_at = tick;
        }
        if (i % 600 == 0) {
            rec.checkpoint();
            checkpoints.u64(w.state_hash());
            REQUIRE(w.check_invariants() == "");
        }
        // The monopoly accumulator never changes after the first advance (D5).
        monopoly_once = monopoly_once && w.monopoly().acc == 100;
    }
    CHECK(monopoly_once);
    CHECK(w.events_dropped() == 0);
    CHECK(w.drain_events().size() == ui.size());
    REQUIRE(w.clock().tick() == 600000);

    // Semantic asserts, derived by hand (independent of the hash).
    // The figures: the worker ends in phase D of the first advance, the carrier in phase C
    // of the advance that reaches tick 7 500 (u16 difference 7 500).
    CHECK(worker_gone == std::uint64_t{50});
    CHECK(carrier_gone == std::uint64_t{7500});
    CHECK(payload<sim::Farm>(w, 0, kFarm).workers_out == 1); // its own worker, sent later
    // Upkeep: 300 bookings (every 2 000 ticks, stamp = t). Player 0: 300 * 22 = 6 600.
    CHECK(player(w, 0).money == 10000 - 300 * 22);
    // Player 1: -2 900, -2 940, -2 980, -3 020 at tick 6 000 (deadline 156 000, the visit
    // that sets it does not test it), bankrupt at the first visit after 156 000 (tick 156
    // 050); the upkeep goes on: -2 900 - 300 * 40.
    CHECK(bankrupt_at == std::uint64_t{156050});
    CHECK(player(w, 1).bankrupt_deadline == 156000);
    CHECK(player(w, 1).money == -2900 - 300 * 40);
    CHECK(player(w, 2).money == 5000);
    CHECK(player(w, 2).state == sim::PlayerState::Alive);
    // Timers: timer 0 wrapped at tick 100 050 (25 - 50) and counted down 499 950 more
    // ticks: 2^32 - 25 - 499 950, still running; timer 2 at 7 + 600 000; timer 4 untouched.
    CHECK(w.timers().active == std::vector<std::uint8_t>{1, 0, 1, 0, 0});
    CHECK(w.timers().counter ==
          std::vector<std::uint32_t>{4294967296U - 25U - 499950U, 0, 600007, 0, 3});
    CHECK(w.timers().last_tick == 600000);
    // Events: three dispatched (two at tick 1 000, one at 400 000), the delayed hit at
    // 700 000 still queued.
    CHECK(dispatched == 3);
    REQUIRE(w.events().q.size() == 1);
    CHECK(w.events().q.begin()->first.first == 700000);
    CHECK(w.events().q.begin()->second.kind == sim::EventKind::DelayedHit);
    // The draws, worked out from the table for seed 508 (cursor 508): the 300 s event draws
    // once per island at game seconds 300, 600, 900 and 1 200 (ticks 150 000 ... 600 000;
    // three islands, 12 draws); the 7.5 minute event fires at 450 000 ms (tick 225 000)
    // and at 900 000 ms (tick 450 000, after that tick's 300 s draws). Draw 4 is r1 = 29 790
    // (% 10 = 0) and draw 5 r2 = 14 924 (% 5 = 4: island 4); draw 12 is r1 = 30 900 (% 10
    // = 0) and draw 13 r2 = 17 713 (% 5 = 3: a null slot, no event).
    CHECK(cursor0 == 508);
    CHECK(draw_after(cursor0, 4) == 29790);
    CHECK(draw_after(cursor0, 5) == 14924);
    CHECK(draw_after(cursor0, 12) == 30900);
    CHECK(draw_after(cursor0, 13) == 17713);
    CHECK(draws[sim::DrawSite::Event300] == 12);
    CHECK(draws[sim::DrawSite::Event750Gate] == 2);
    CHECK(draws[sim::DrawSite::Event750Island] == 2);
    CHECK(w.rng().cursor() == cursor0 + 16);
    CHECK(w.event750_acc() == 300000); // 1 200 000 - 900 000
    std::vector<sim::SimEvent> fired, timers_out;
    for (const sim::SimEvent& e : ui) {
        if (e.kind == sim::SimEventKind::Event15a) {
            fired.push_back(e);
        } else if (e.kind == sim::SimEventKind::TimerExpired) {
            timers_out.push_back(e);
        }
    }
    REQUIRE(fired.size() == 1);
    CHECK(fired[0].tick == 225000);
    CHECK(fired[0].x == 308);
    CHECK(fired[0].y == 308);
    CHECK(timers_out.empty()); // the wrapped timer never stopped
    // The monopoly table of the first advance (see "monopoly once and the rule").
    CHECK(w.monopoly().entries[6].player == sim::PlayerId{0});
    CHECK(w.monopoly().entries[16].player == sim::PlayerId{1});
    CHECK_FALSE(w.monopoly().entries[0].player.has_value());

    // The log is byte-identical to the committed one (ANNO_UPDATE_GOLDEN=1 rewrites it).
    const sim::ReplayLog log = rec.log();
    auto written = sim::write_log(log);
    REQUIRE_MESSAGE(written.ok(), (written.ok() ? std::string() : written.error()));
    if (anno::test::update_mode()) {
        std::ofstream out(g4_path(), std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(written.value().data()),
                  static_cast<std::streamsize>(written.value().size()));
        MESSAGE("wrote " << g4_path().string());
    }
    const std::vector<std::uint8_t> committed = read_bytes(g4_path());
    CHECK_MESSAGE(committed == written.value(), "the recorded G4 log differs from "
                                                    << g4_path().string()
                                                    << " (ANNO_UPDATE_GOLDEN=1 rewrites it)");
    // The committed log replays on a world rebuilt from its initial save: 21 of 21.
    auto read = sim::read_log(committed);
    REQUIRE_MESSAGE(read.ok(), (read.ok() ? std::string() : read.error()));
    auto fresh = sim::World::load(read.value().initial_save, t.registry);
    REQUIRE_MESSAGE(fresh.ok(), (fresh.ok() ? std::string() : fresh.error()));
    sim::World replayed = std::move(fresh).value();
    auto outcome = sim::replay(replayed, read.value());
    REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
    CHECK(outcome.value().reproduced());
    CHECK(outcome.value().checkpoints_total == 21);
    CHECK(outcome.value().checkpoints_matched == 21);
    CHECK(replayed.state_hash() == w.state_hash());

    // Goldens (self-baselined, goldens.txt; the asserts above do not depend on them).
    check_golden_line("s12_g4_checkpoints", checkpoints.value());
    check_golden_line("s12_g4_end", w.state_hash());
    check_golden_line("s12_g4_events", hash_events(ui));
}
