// The figure graph and the expedition tree of the ship-start load (task MAP-2;
// docs/sim/ship-start-integration.md §2). Synthetic: no game files.
#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/figure.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim/world.hpp>

#include "island_helpers.hpp"
#include "ship_helpers.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;

namespace {

using ship_test::goods;
using ship_test::kDonkey;
using ship_test::kScout;
using ship_test::kShip;
using ship_test::ship_registry;

/// The hand-derived baseline: wood 50, food 10, tools 50, tools 30 (HOLZ, NAHRUNG, WERKZEUG).
std::vector<sim::ShipHold> baseline_holds(const sim::TypeRegistry& types) {
    return {{goods(types, "HOLZ"), 50},
            {goods(types, "NAHRUNG"), 10},
            {goods(types, "WERKZEUG"), 50},
            {goods(types, "WERKZEUG"), 30}};
}

/// Five seats, each a ship (no parent), a scout (parent the ship, crew slot 0) and a donkey
/// (parent the scout), at the centre of tile (10 + 20 * seat, 100): the figures are in spec
/// order, so seat s holds ids 3s + 1, 3s + 2, 3s + 3.
sim::ScenarioSpec expedition_spec(const sim::TypeRegistry& types) {
    sim::ScenarioSpec spec = syn::empty_scenario(77);
    for (std::uint32_t seat = 0; seat < 5; ++seat) {
        sim::ScenarioSpec::PlayerSpec p;
        p.id = sim::PlayerId{seat};
        p.money = 30000;
        spec.players.push_back(p);
    }
    for (std::uint32_t seat = 0; seat < 5; ++seat) {
        const sim::FixedPos pos{static_cast<std::int32_t>(256 * (10 + 20 * seat) + 128),
                                256 * 100 + 128};
        const std::uint8_t dir16 = static_cast<std::uint8_t>(12 - 4 * (seat % 4)); // 12, 8, 4, 0
        sim::ScenarioSpec::FigureSpec ship;
        ship.kind = kShip;
        ship.owner = sim::PlayerId{seat};
        ship.pos = pos;
        ship.dir16 = dir16;
        ship.holds = baseline_holds(types);
        spec.figures.push_back(ship);
        const auto ship_index = static_cast<std::uint32_t>(spec.figures.size() - 1);
        sim::ScenarioSpec::FigureSpec scout = ship;
        scout.kind = kScout;
        scout.holds.clear();
        scout.parent = ship_index;
        scout.crew_slot = 0;
        spec.figures.push_back(scout);
        sim::ScenarioSpec::FigureSpec donkey = scout;
        donkey.kind = kDonkey;
        donkey.parent = ship_index + 1;
        donkey.crew_slot.reset();
        spec.figures.push_back(donkey);
    }
    return spec;
}

sim::World make_expedition_world(const std::shared_ptr<const sim::TypeRegistry>& types) {
    auto w = sim::World::create(expedition_spec(*types), types);
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    return std::move(w).value();
}

/// A registry-only fixture for the graph rules: ids 1..n, set by the caller.
struct Graph {
    sim::SimConfig cfg;
    sim::Rules rules;
    sim::FigureRegistry reg = sim::make_figure_registry(sim::SimConfig{});
    sim::IdCounters ids;

    void add(std::uint64_t id, std::uint32_t owner = 0) {
        sim::Figure f;
        f.id = sim::FigureId{id};
        f.owner = sim::PlayerId{owner};
        REQUIRE(sim::add_figure(reg, f, cfg, rules) != sim::kNoSlot);
        ids.next_figure = std::max(ids.next_figure, id + 1);
    }
    sim::Figure& at(std::uint64_t id) {
        sim::Figure* f = sim::find_figure(reg, sim::FigureId{id});
        REQUIRE(f != nullptr);
        return *f;
    }
    [[nodiscard]] std::string check() const { return sim::validate_figures(reg, ids); }
};

bool contains(const std::string& text, const char* needle) {
    return text.find(needle) != std::string::npos;
}

} // namespace

TEST_CASE("figure graph: the expedition tree is valid and a donkey on the ship is also valid") {
    Graph g;
    g.add(1);
    g.add(2);
    g.add(3);
    g.at(2).parent = sim::FigureId{1};
    g.at(2).crew_slot = 0;
    g.at(3).parent = sim::FigureId{2};
    CHECK(g.check().empty());
    // Generic graph validity is not the baseline expectation: the donkey on the ship (crew
    // slot 1) is a valid graph. The loader's baseline check is stricter (see the loader test).
    g.at(3).parent = sim::FigureId{1};
    g.at(3).crew_slot = 1;
    CHECK(g.check().empty());
}

TEST_CASE("figure graph: facing, parents, owners and crew slots are validated") {
    {
        Graph g;
        g.add(1);
        g.at(1).dir16 = 16;
        CHECK(contains(g.check(), "dir16"));
        g.at(1).dir16 = 15;
        CHECK(g.check().empty());
    }
    {
        Graph g;
        g.add(1);
        g.at(1).parent = sim::FigureId{9};
        CHECK(contains(g.check(), "is not in the registry"));
    }
    {
        Graph g;
        g.add(1);
        g.at(1).parent = sim::FigureId{1};
        CHECK(contains(g.check(), "its own parent"));
    }
    {
        Graph g;
        g.add(1);
        g.add(2);
        g.at(1).parent = sim::FigureId{2};
        g.at(2).parent = sim::FigureId{1};
        CHECK(contains(g.check(), "cycle"));
    }
    {
        Graph g; // a three-cycle behind a tail that is itself fine
        for (std::uint64_t i = 1; i <= 5; ++i) {
            g.add(i);
        }
        g.at(5).parent = sim::FigureId{4};
        g.at(4).parent = sim::FigureId{3};
        g.at(3).parent = sim::FigureId{2};
        g.at(2).parent = sim::FigureId{1};
        CHECK(g.check().empty());
        g.at(1).parent = sim::FigureId{3};
        CHECK(contains(g.check(), "cycle"));
    }
    {
        Graph g;
        g.add(1, 0);
        g.add(2, 1);
        g.at(2).parent = sim::FigureId{1};
        CHECK(contains(g.check(), "owner"));
    }
    {
        Graph g;
        g.add(1);
        g.at(1).crew_slot = 0;
        CHECK(contains(g.check(), "no parent"));
    }
    {
        Graph g;
        g.add(1);
        g.add(2);
        g.add(3);
        g.at(2).parent = sim::FigureId{1};
        g.at(3).parent = sim::FigureId{1};
        g.at(2).crew_slot = 0;
        g.at(3).crew_slot = 0;
        CHECK(contains(g.check(), "used twice"));
        g.at(3).crew_slot = 1;
        CHECK(g.check().empty());
    }
    {
        // The existing invariant stays: id 0 is not a figure id, and next_figure bounds ids.
        Graph g;
        sim::Figure f;
        f.id = sim::FigureId{0};
        g.reg.list.insert(f, sim::SlotParams{100, g.rules.slot_policy});
        g.ids.next_figure = 5;
        CHECK(contains(g.check(), "is not in 1 .. next_figure"));
    }
}

TEST_CASE("figure graph: validation is not quadratic in the depth of the nesting") {
    // A chain of 200 000 figures, each the parent of the next. A check that walks every
    // ancestor chain from every figure would do 2 * 10^10 steps and never finish here.
    Graph g;
    constexpr std::uint64_t n = 200000;
    for (std::uint64_t i = 1; i <= n; ++i) {
        g.add(i);
        if (i > 1) {
            g.at(i).parent = sim::FigureId{i - 1};
        }
    }
    CHECK(g.check().empty());
    g.at(1).parent = sim::FigureId{n}; // closes the ring
    CHECK(contains(g.check(), "cycle"));
}

TEST_CASE("figure graph: removing a parent leaves its child detectable, not silent") {
    Graph g;
    g.add(1);
    g.add(2);
    g.at(2).parent = sim::FigureId{1};
    REQUIRE(g.check().empty());
    REQUIRE(sim::remove_figure(g.reg, sim::FigureId{1}).has_value());
    CHECK(contains(g.check(), "is not in the registry"));
}

TEST_CASE("expedition tree: ids, parents, positions, facing and holds from a scenario") {
    const auto types = ship_registry();
    const sim::World w = make_expedition_world(types);
    CHECK(w.check_invariants().empty());
    CHECK(w.ids().next_figure == 16);
    CHECK(w.figures().by_id.size() == 15);
    for (std::uint32_t seat = 0; seat < 5; ++seat) {
        CAPTURE(seat);
        const std::uint64_t base = 3 * seat + 1; // hand-derived: 1, 4, 7, 10, 13
        const sim::Figure* ship = sim::find_figure(w.figures(), sim::FigureId{base});
        const sim::Figure* scout = sim::find_figure(w.figures(), sim::FigureId{base + 1});
        const sim::Figure* donkey = sim::find_figure(w.figures(), sim::FigureId{base + 2});
        REQUIRE(ship != nullptr);
        REQUIRE(scout != nullptr);
        REQUIRE(donkey != nullptr);
        CHECK(ship->kind == kShip);
        CHECK(scout->kind == kScout);
        CHECK(donkey->kind == kDonkey);
        CHECK_FALSE(ship->parent.valid());
        CHECK(scout->parent == ship->id);
        CHECK(donkey->parent == scout->id);
        CHECK_FALSE(ship->crew_slot.has_value());
        CHECK(scout->crew_slot == std::optional<std::uint32_t>(0));
        CHECK_FALSE(donkey->crew_slot.has_value());
        for (const sim::Figure* f : {ship, scout, donkey}) {
            CHECK(f->owner == sim::PlayerId{seat});
            CHECK_FALSE(f->home.valid());
            CHECK(f->pos.x == static_cast<std::int32_t>(256 * (10 + 20 * seat) + 128));
            CHECK(f->pos.y == 256 * 100 + 128);
            CHECK(f->dir16 == 12 - 4 * (seat % 4));
            CHECK(f->cargo_goods == 0);
            CHECK(f->cargo_88 == 0);
            CHECK_FALSE(f->expiry_stamp.has_value());
        }
        // Four holds in file order, the two tools entries unmerged; only the ship has any.
        REQUIRE(ship->holds.size() == 4);
        CHECK(ship->holds[0] == sim::ShipHold{goods(*types, "HOLZ"), 50});
        CHECK(ship->holds[1] == sim::ShipHold{goods(*types, "NAHRUNG"), 10});
        CHECK(ship->holds[2] == sim::ShipHold{goods(*types, "WERKZEUG"), 50});
        CHECK(ship->holds[3] == sim::ShipHold{goods(*types, "WERKZEUG"), 30});
        CHECK(scout->holds.empty());
        CHECK(donkey->holds.empty());
        CHECK(w.players().find(sim::PlayerId{seat})->money == 30000);
    }
    // No city, no island, nothing drawn from the generator.
    CHECK(w.islands().empty());
    CHECK(w.rng().cursor() == 77 % w.rng().size());
}

TEST_CASE("expedition tree: the same scenario gives the same world, on any executor") {
    const auto types = ship_registry();
    const std::uint64_t first = make_expedition_world(types).state_hash();
    CHECK(make_expedition_world(types).state_hash() == first);
    // The order of the holds is state: swapping the two tools entries (50 then 30) gives a
    // different world although the same goods and tons are aboard.
    sim::ScenarioSpec swapped = expedition_spec(*types);
    std::swap(swapped.figures[0].holds[2], swapped.figures[0].holds[3]);
    CHECK(sim::World::create(swapped, types).value().state_hash() != first);
    // Merging them into one hold of 80 t is also a different world.
    sim::ScenarioSpec merged = expedition_spec(*types);
    merged.figures[0].holds[2].tons = 80;
    merged.figures[0].holds.pop_back();
    CHECK(sim::World::create(merged, types).value().state_hash() != first);
}

TEST_CASE("expedition tree: save, load, save is byte-identical and the derived index rebuilds") {
    const auto types = ship_registry();
    const sim::World w = make_expedition_world(types);
    const std::vector<std::uint8_t> bytes = w.save();
    auto loaded = sim::World::load(bytes, types);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    CHECK(loaded.value().state_hash() == w.state_hash());
    CHECK(loaded.value().save() == bytes);
    CHECK(loaded.value().check_invariants().empty());
    CHECK(loaded.value().ids().next_figure == 16);
    CHECK(loaded.value().players().find(sim::PlayerId{4})->money == 30000);
    // by_id is derived: emptied, it is rebuilt equal, and the hash never saw it.
    sim::World copy = loaded.value();
    const std::uint64_t hash = copy.state_hash();
    sim::WorldTestAccess::figures(copy).by_id.clear();
    CHECK(copy.state_hash() == hash);
    sim::rebuild_figure_index(sim::WorldTestAccess::figures(copy));
    CHECK(copy.figures().by_id == loaded.value().figures().by_id);
    CHECK(copy.check_invariants().empty());
}

TEST_CASE("expedition tree: a stream of another version or with a corrupt reference is refused") {
    const auto types = ship_registry();
    const sim::World w = make_expedition_world(types);
    const std::vector<std::uint8_t> good = w.save();

    // The version: the state stream starts with u32 kStateHashVersion, u32 PRNG timeline.
    const std::vector<std::uint8_t> marker = {
        static_cast<std::uint8_t>(sim::kStateHashVersion),    0, 0, 0,
        static_cast<std::uint8_t>(sim::kPrngTimelineVersion), 0, 0, 0};
    const auto at = std::search(good.begin(), good.end(), marker.begin(), marker.end());
    REQUIRE(at != good.end());
    std::vector<std::uint8_t> old_version = good;
    old_version[static_cast<std::size_t>(at - good.begin())] = 115;
    CHECK_FALSE(sim::World::load(old_version, types).ok());

    // A parent that is not in the registry: written through the test access, saved, refused.
    sim::World broken = w;
    sim::WorldTestAccess::figures(broken).list.slots[1]->parent = sim::FigureId{999};
    auto refused = sim::World::load(broken.save(), types);
    REQUIRE_FALSE(refused.ok());
    CHECK(contains(refused.error(), "parent"));

    // A cycle.
    sim::World ring = w;
    sim::WorldTestAccess::figures(ring).list.slots[0]->parent = sim::FigureId{3};
    refused = sim::World::load(ring.save(), types);
    REQUIRE_FALSE(refused.ok());
    CHECK(contains(refused.error(), "cycle"));

    // A hold of goods the registry does not have, and a facing beyond 15.
    sim::World odd = w;
    sim::WorldTestAccess::figures(odd).list.slots[0]->holds[0].goods = 100000;
    refused = sim::World::load(odd.save(), types);
    REQUIRE_FALSE(refused.ok());
    CHECK(contains(refused.error(), "holds[0].goods"));
    sim::World turned = w;
    sim::WorldTestAccess::figures(turned).list.slots[0]->dir16 = 16;
    refused = sim::World::load(turned.save(), types);
    REQUIRE_FALSE(refused.ok());
    CHECK(contains(refused.error(), "dir16"));
}

TEST_CASE("expedition tree: a hostile hold count cannot size a collection") {
    // The hold list is read one element at a time against the remaining input: a count of
    // 2^40 + 4 in a stream that holds four holds fails before anything is allocated. (The
    // save frame's checksum would catch the damage first, so the flat state stream is used.)
    const auto types = ship_registry();
    sim::World w = make_expedition_world(types);
    std::vector<std::uint8_t> bytes = island_test::flat_stream(w);
    // The first ship's holds: u64 count 4, then (goods, tons) of HOLZ, 50.
    const std::uint32_t holz = goods(*types, "HOLZ");
    std::vector<std::uint8_t> pattern = {4, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < 4; ++i) {
        pattern.push_back(static_cast<std::uint8_t>((holz >> (8 * i)) & 0xFF));
    }
    for (const unsigned b : {50U, 0U, 0U, 0U}) {
        pattern.push_back(static_cast<std::uint8_t>(b));
    }
    const auto at = std::search(bytes.begin(), bytes.end(), pattern.begin(), pattern.end());
    REQUIRE(at != bytes.end());
    REQUIRE(island_test::read_flat(w, bytes).empty()); // the unmodified stream reads whole
    bytes[static_cast<std::size_t>(at - bytes.begin()) + 5] = 1; // count = 2^40 + 4
    const std::string refused = island_test::read_flat(w, bytes);
    CHECK_MESSAGE(contains(refused, "count exceeds the remaining input"), refused);
    CHECK(contains(refused, "figure[0]")); // the scope path names the figure
}

TEST_CASE("expedition tree: a scenario that names content or links the registry lacks is refused") {
    const auto types = ship_registry();
    const auto refused_with = [&](auto mutate, const char* path) {
        sim::ScenarioSpec spec = expedition_spec(*types);
        mutate(spec);
        auto r = sim::World::create(spec, types);
        REQUIRE_FALSE(r.ok());
        CHECK_MESSAGE(contains(r.error(), path), r.error());
    };
    refused_with([](sim::ScenarioSpec& s) { s.figures[1].kind = 99999; }, "figures[1].kind");
    refused_with([](sim::ScenarioSpec& s) { s.figures[0].owner = sim::PlayerId{9}; },
                 "figures[0].owner");
    refused_with([](sim::ScenarioSpec& s) { s.figures[2].dir16 = 16; }, "figures[2].dir16");
    refused_with([](sim::ScenarioSpec& s) { s.figures[0].pos.x = 256 * 512; }, "figures[0].pos");
    refused_with([](sim::ScenarioSpec& s) { s.figures[0].pos.y = -1; }, "figures[0].pos");
    refused_with([](sim::ScenarioSpec& s) { s.figures[1].parent = 2U; }, "figures[1].parent");
    refused_with([](sim::ScenarioSpec& s) { s.figures[1].parent = 1U; }, "figures[1].parent");
    refused_with([](sim::ScenarioSpec& s) { s.figures[3].owner = sim::PlayerId{0}; },
                 "figures[4].owner");
    refused_with([](sim::ScenarioSpec& s) { s.figures[0].crew_slot = 0U; }, "figures[0].crew_slot");
    refused_with([](sim::ScenarioSpec& s) { s.figures[0].holds[1].goods = 100000; },
                 "figures[0].holds[1].goods");
}

TEST_CASE("expedition tree: a failed create leaves an existing world unchanged") {
    const auto types = ship_registry();
    sim::World live = make_expedition_world(types);
    const std::uint64_t before = live.state_hash();
    const std::vector<std::uint8_t> saved = live.save();
    sim::ScenarioSpec bad = expedition_spec(*types);
    bad.figures[4].kind = 424242;
    auto r = sim::World::create(bad, types);
    REQUIRE_FALSE(r.ok());
    CHECK(live.state_hash() == before);
    CHECK(live.save() == saved);
}

TEST_CASE("expedition tree: an idle advance leaves the expedition where it was") {
    const auto types = ship_registry();
    sim::World w = make_expedition_world(types);
    const sim::FigureRegistry before = w.figures();
    for (int i = 0; i < 500; ++i) {
        w.advance(50);
    }
    CHECK(w.check_invariants().empty());
    CHECK(w.figures().by_id == before.by_id);
    REQUIRE(w.figures().list.slots.size() == before.list.slots.size());
    for (std::size_t s = 0; s < before.list.slots.size(); ++s) {
        REQUIRE(w.figures().list.slots[s].has_value() == before.list.slots[s].has_value());
        if (!before.list.slots[s].has_value()) {
            continue;
        }
        const sim::Figure& a = *before.list.slots[s];
        const sim::Figure& b = *w.figures().list.slots[s];
        CHECK(a.id == b.id);
        CHECK(a.pos.x == b.pos.x);
        CHECK(a.pos.y == b.pos.y);
        CHECK(a.dir16 == b.dir16);
        CHECK(a.parent == b.parent);
        CHECK(a.crew_slot == b.crew_slot);
        CHECK(a.holds == b.holds);
        CHECK_FALSE(b.expiry_stamp.has_value());
    }
    CHECK(w.ids().next_figure == 16);
    CHECK(w.players().find(sim::PlayerId{2})->money == 30000);
}
