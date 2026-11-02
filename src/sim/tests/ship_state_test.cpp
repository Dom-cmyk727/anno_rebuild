// The figure graph and the expedition tree of the ship-start load (task MAP-2;
// docs/sim/ship-start-integration.md §2). Synthetic: no game files.
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/archive.hpp>
#include <anno/sim/city_creation.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/founding_payment.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/state_fields.hpp>
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

/// The hand-derived baseline: wood 50, food 10, tools 50, tools 30 tons (HOLZ, NAHRUNG, WERKZEUG),
/// i.e. 50 * 256 = 12800, 10 * 256 = 2560, 12800 and 30 * 256 = 7680 raw units.
std::vector<sim::ShipHold> baseline_holds(const sim::TypeRegistry& types) {
    return {{goods(types, "HOLZ"), 12800},
            {goods(types, "NAHRUNG"), 2560},
            {goods(types, "WERKZEUG"), 12800},
            {goods(types, "WERKZEUG"), 7680}};
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
        CHECK(ship->holds[0] == sim::ShipHold{goods(*types, "HOLZ"), 12800});
        CHECK(ship->holds[1] == sim::ShipHold{goods(*types, "NAHRUNG"), 2560});
        CHECK(ship->holds[2] == sim::ShipHold{goods(*types, "WERKZEUG"), 12800});
        CHECK(ship->holds[3] == sim::ShipHold{goods(*types, "WERKZEUG"), 7680});
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
    merged.figures[0].holds[2].amount_88 = 80 * 256;
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

    // Unsupported versions (header-only edits with a valid checksum; each reaches its own check).
    island_test::check_unsupported_versions(good, types, {115, 120});

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
    // The first ship's holds: u64 count 4, then (u32 goods, u64 amount) of HOLZ, 50 t = 12800 =
    // 0x3200 raw units, little endian.
    const std::uint32_t holz = goods(*types, "HOLZ");
    std::vector<std::uint8_t> pattern = {4, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < 4; ++i) {
        pattern.push_back(static_cast<std::uint8_t>((holz >> (8 * i)) & 0xFF));
    }
    for (const unsigned b : {0x00U, 0x32U, 0U, 0U, 0U, 0U, 0U, 0U}) {
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

// ---- Exact ship cargo (package B, stream 117) ----

namespace {

std::string hex(const std::vector<std::uint8_t>& v) {
    static const char* d = "0123456789abcdef";
    std::string s;
    for (const std::uint8_t b : v) {
        s += d[b >> 4];
        s += d[b & 15];
    }
    return s;
}

/// The hold list as the figure record writes it (u64 count, 12 bytes per hold).
std::vector<std::uint8_t> encode(std::vector<sim::ShipHold> holds) {
    sim::WriteArchive w;
    sim::ar::seq(w, holds, 12, [&](auto& h) { fields(w, h); });
    return w.take();
}

std::map<std::string, std::string> fixture() {
    const std::filesystem::path p =
        std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "ship_holds.txt";
    std::ifstream in(p);
    REQUIRE_MESSAGE(in.good(), "missing fixture " << p.string());
    std::map<std::string, std::string> out;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream ss(line);
        std::string name, bytes;
        ss >> name >> bytes;
        out[name] = bytes;
    }
    return out;
}

/// A world whose first ship carries `holds` (the expedition of ship_state_test, five seats).
sim::World world_with_holds(const std::shared_ptr<const sim::TypeRegistry>& types,
                            std::vector<sim::ShipHold> holds) {
    sim::ScenarioSpec spec = expedition_spec(*types);
    spec.figures[0].holds = std::move(holds);
    auto w = sim::World::create(spec, types);
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    return std::move(w).value();
}

} // namespace

TEST_CASE("ship holds: the wire bytes equal the independent transcription") {
    const std::map<std::string, std::string> want = fixture();
    // Expected amounts are written here by hand (raw units), not read back from the fixture.
    const std::map<std::string, std::vector<sim::ShipHold>> cases = {
        {"empty", {}},
        {"one-raw-unit", {{3, 1}}},
        {"whole-ton", {{3, 256}}},
        {"baseline-order", {{5, 12800}, {6, 2560}, {9, 12800}, {9, 7680}}},
        {"baseline-swapped-tools", {{5, 12800}, {6, 2560}, {9, 7680}, {9, 12800}}},
        {"u32-max-tons", {{7, std::uint64_t{0xFFFFFFFFU} * 256}}},
        {"u64-max", {{1, std::numeric_limits<std::uint64_t>::max()}}},
    };
    REQUIRE(want.size() == cases.size());
    for (const auto& [name, holds] : cases) {
        INFO(name);
        REQUIRE(want.count(name) == 1);
        CHECK(hex(encode(holds)) == want.at(name));
        // And back: the bytes read as the same holds.
        const std::vector<std::uint8_t> bytes = encode(holds);
        sim::ReadArchive r(bytes);
        std::vector<sim::ShipHold> back;
        sim::ar::seq(r, back, 12, [&](auto& h) { fields(r, h); });
        CHECK_FALSE(r.failed());
        CHECK(r.remaining() == 0);
        CHECK(back == holds);
    }
    // Negative control: swapped duplicate holds are different bytes, and a one-unit change too.
    CHECK(want.at("baseline-order") != want.at("baseline-swapped-tools"));
    CHECK(encode({{3, 1}}) != encode({{3, 2}}));
}

TEST_CASE("ship holds: whole tons convert exactly and overflow is refused") {
    CHECK(sim::hold_units_from_tons(0) == std::optional<std::uint64_t>(0));
    CHECK(sim::hold_units_from_tons(50) == std::optional<std::uint64_t>(12800));
    CHECK(sim::hold_units_from_tons(10) == std::optional<std::uint64_t>(2560));
    CHECK(sim::hold_units_from_tons(30) == std::optional<std::uint64_t>(7680));
    CHECK(sim::hold_units_from_tons(0xFFFFFFFFULL) ==
          std::optional<std::uint64_t>(std::uint64_t{0xFFFFFFFFU} * 256));
    constexpr std::uint64_t top = std::numeric_limits<std::uint64_t>::max() / 256;
    CHECK(sim::hold_units_from_tons(top) == std::optional<std::uint64_t>(top * 256));
    CHECK_FALSE(sim::hold_units_from_tons(top + 1).has_value());
    CHECK_FALSE(sim::hold_units_from_tons(std::numeric_limits<std::uint64_t>::max()).has_value());
}

TEST_CASE("ship holds: one raw unit, large amounts and duplicate order survive save and load") {
    const auto types = ship_registry();
    const std::vector<sim::ShipHold> holds = {
        {goods(*types, "HOLZ"), 1},
        {goods(*types, "WERKZEUG"), 12800},
        {goods(*types, "WERKZEUG"), 7680},
        {goods(*types, "NAHRUNG"), std::uint64_t{0xFFFFFFFFU} * 256},
    };
    sim::World w = world_with_holds(types, holds);
    const std::vector<std::uint8_t> bytes = w.save();
    auto loaded = sim::World::load(bytes, types);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    const sim::Figure* ship = sim::find_figure(loaded.value().figures(), sim::FigureId{1});
    REQUIRE(ship != nullptr);
    CHECK(ship->holds == holds);
    CHECK(loaded.value().state_hash() == w.state_hash());
    CHECK(loaded.value().save() == bytes);
    CHECK(loaded.value().check_invariants().empty());

    // The amount is state to the unit: one raw unit more is another world.
    std::vector<sim::ShipHold> plus = holds;
    plus[0].amount_88 += 1;
    CHECK(world_with_holds(types, plus).state_hash() != w.state_hash());
    // The widest representable amount round-trips as well.
    const std::vector<sim::ShipHold> widest = {
        {goods(*types, "HOLZ"), std::numeric_limits<std::uint64_t>::max()}};
    sim::World ww = world_with_holds(types, widest);
    auto wl = sim::World::load(ww.save(), types);
    REQUIRE_MESSAGE(wl.ok(), (wl.ok() ? std::string() : wl.error()));
    CHECK(sim::find_figure(wl.value().figures(), sim::FigureId{1})->holds == widest);
}

TEST_CASE("ship holds: a stream of the whole-ton layout or a truncated hold fails") {
    const auto types = ship_registry();
    sim::World w =
        world_with_holds(types, {{goods(*types, "HOLZ"), 12800}, {goods(*types, "NAHRUNG"), 5}});
    // The version: a state/save of the previous layout (116, 8-byte holds) is refused, never
    // reinterpreted as 1/256 ton.
    const std::vector<std::uint8_t> good = w.save();
    // 116 had whole-ton holds of 8 bytes, 117 raw amounts without the island lock: both numbers are
    // refused by the version checks (the bytes are this build's layout with only the number
    // changed).
    island_test::check_unsupported_versions(good, types, {116, 117, 118, 119, 120, 121, 122, 123});
    CHECK(sim::kStateHashVersion == 125);

    // Truncation of the flat stream inside a hold amount fails and the error names the figure.
    std::vector<std::uint8_t> flat = island_test::flat_stream(w);
    std::vector<std::uint8_t> pattern = {2, 0, 0, 0, 0, 0, 0, 0};
    const std::uint32_t holz = goods(*types, "HOLZ");
    for (int i = 0; i < 4; ++i) {
        pattern.push_back(static_cast<std::uint8_t>((holz >> (8 * i)) & 0xFF));
    }
    for (const unsigned b : {0x00U, 0x32U, 0U, 0U, 0U, 0U, 0U, 0U}) {
        pattern.push_back(static_cast<std::uint8_t>(b));
    }
    const auto p = std::search(flat.begin(), flat.end(), pattern.begin(), pattern.end());
    REQUIRE(p != flat.end());
    sim::World target = world_with_holds(types, {});
    REQUIRE(island_test::read_flat(target, flat).empty()); // the whole stream reads
    // Cut the stream in the middle of the first hold's u64 amount (4 bytes of it present).
    std::vector<std::uint8_t> cut(flat.begin(), p + 8 + 4 + 4);
    sim::World target2 = world_with_holds(types, {});
    CHECK_FALSE(island_test::read_flat(target2, cut).empty());
    // And a stream whose second hold lacks its last byte.
    std::vector<std::uint8_t> cut2(flat.begin(), p + 8 + 12 + 12 - 1);
    sim::World target3 = world_with_holds(types, {});
    CHECK_FALSE(island_test::read_flat(target3, cut2).empty());
}

TEST_CASE(
    "canary: lock and fractional cargo survive save and load, a prepared payment mutates nothing") {
    const auto types = ship_registry();
    sim::ScenarioSpec spec = expedition_spec(*types);
    spec.islands.push_back(syn::plain_island(0, 40, 24, 6, 4));
    spec.islands.back().lock = 1;
    const sim::GoodsId wood = goods(*types, "HOLZ");
    const sim::GoodsId tools = goods(*types, "WERKZEUG");
    // 50 t and one raw unit of wood, then the two tools holds (50 t + 30 t).
    spec.figures[0].holds = {{wood, 12801}, {tools, 12800}, {tools, 7680}};
    auto created = sim::World::create(spec, types);
    REQUIRE_MESSAGE(created.ok(), (created.ok() ? std::string() : created.error()));
    auto loaded_result = sim::World::load(created.value().save(), types);
    REQUIRE_MESSAGE(loaded_result.ok(),
                    (loaded_result.ok() ? std::string() : loaded_result.error()));
    sim::World world = std::move(loaded_result).value();
    CHECK(world.state_hash() == created.value().state_hash());
    REQUIRE(world.islands()[0].has_value());
    CHECK(world.islands()[0]->locked());

    const sim::Figure* ship = sim::find_figure(world.figures(), sim::FigureId{1});
    REQUIRE(ship != nullptr);
    const sim::Player* owner = world.players().find(sim::PlayerId{0});
    REQUIRE(owner != nullptr);
    sim::BuildingType type;
    type.cost_gold = 1000;
    type.cost_goods = {{wood, 12801}, {tools, 20480}}; // hand-derived: all of it
    const std::uint64_t hash = world.state_hash();
    const std::vector<std::uint8_t> bytes = world.save();
    const auto plan = sim::payment::prepare(ship->holds, owner->money, type);
    REQUIRE(plan.ok());
    CHECK(plan.value().holds ==
          std::vector<sim::ShipHold>{{0, 0}, {0, 0}, {0, 0}}); // wood 12801 and both tools holds
    CHECK(plan.value().money_after == 29000);
    CHECK(plan.value().gold_charged == 1000);
    // Nothing of the world moved: not the holds, not the gold, not the hash or the saved bytes.
    CHECK(ship->holds.size() == 3);
    CHECK(ship->holds[0] == sim::ShipHold{wood, 12801});
    CHECK(owner->money == 30000);
    CHECK(world.state_hash() == hash);
    CHECK(world.save() == bytes);
    // One raw unit more wood than the ship carries is a refusal naming it.
    type.cost_goods = {{wood, 12802}};
    const auto refused = sim::payment::prepare(ship->holds, owner->money, type);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error() ==
          sim::payment::Refusal{sim::payment::Status::InsufficientGoods, wood, 12802, 12801});
    CHECK(world.state_hash() == hash);
}

TEST_CASE("canary: payment, city and Kontor preparation are non-mutating, the city operation holds "
          "its invariants") {
    // Connects the ship cargo and payment preparation (founding_payment.hpp) with the city
    // prerequisites (city_creation.hpp). It founds NO warehouse: nothing here applies a payment,
    // builds, claims territory or issues a command. docs/sim/city-creation.md.
    const auto types = ship_registry();
    sim::ScenarioSpec spec = expedition_spec(*types);
    spec.islands.push_back(syn::plain_island(0, 40, 24, 6, 4));
    const sim::GoodsId wood = goods(*types, "HOLZ");
    const sim::GoodsId tools = goods(*types, "WERKZEUG");
    spec.figures[0].holds = {{wood, 12801}, {tools, 12800}, {tools, 7680}};
    spec.islands.back().kontor_counter = 12;
    auto created = sim::World::create(spec, types);
    REQUIRE_MESSAGE(created.ok(), (created.ok() ? std::string() : created.error()));
    sim::World world = std::move(created).value();
    sim::WorldTestAccess::rng(world).set_cursor(0);

    const sim::Figure* ship = sim::find_figure(world.figures(), sim::FigureId{1});
    REQUIRE(ship != nullptr);
    const sim::PlayerId owner = ship->owner;
    const sim::Player* player = world.players().find(owner);
    REQUIRE(player != nullptr);
    sim::BuildingType type;
    type.cost_gold = 1000;
    type.cost_goods = {{wood, 12801}, {tools, 20480}};

    // 1. All three preparations on the live world, none of them applied.
    const std::vector<std::uint8_t> bytes = world.save();
    const std::uint64_t hash = world.state_hash();
    const std::uint32_t rng_cursor = world.rng().cursor();
    const auto pay = sim::payment::prepare(ship->holds, player->money, type);
    REQUIRE(pay.ok());
    const std::vector<std::string> names = {"Alpha", "Beta", "Gamma"};
    sim::CityRequest rq;
    rq.island = sim::IslandId{0};
    rq.owner = owner;
    rq.stamp = 99;
    rq.names = std::span<const std::string>(names);
    const auto city = sim::CityCreation::prepare(world, rq);
    REQUIRE(city.ok());
    const auto kontor = sim::CityCreation::prepare_kontor(world, sim::IslandId{0});
    REQUIRE(kontor.ok());
    CHECK(pay.value().money_after == 29000);
    CHECK(city.value().number.v == 0);
    CHECK(city.value().name == "Gamma"); // T[1] = 18467, 18467 mod 3 = 2
    CHECK(kontor.value().id == 12);
    CHECK(world.save() == bytes);
    CHECK(world.state_hash() == hash);
    CHECK(world.rng().cursor() == rng_cursor);

    // 2. The internal city operation on a copy, separately: invariants, the one PRNG step, the
    // Kontor identifier, the persisted state. The payment plan is not applied (the cargo stays).
    sim::World copy = world;
    REQUIRE(sim::CityCreation::commit(copy, city.value()).ok());
    auto id = sim::CityCreation::commit_kontor(copy, kontor.value());
    REQUIRE(id.ok());
    CHECK(id.value() == 12);
    CHECK(copy.rng().cursor() == rng_cursor + 1);
    CHECK(copy.check_invariants().empty());
    const sim::Island& is = *copy.islands()[0];
    CHECK(is.city(sim::CityNo{0}) != nullptr);
    CHECK(is.city(sim::CityNo{0})->owner == owner);
    CHECK(is.city(sim::CityNo{0})->name == "Gamma");
    CHECK(is.city_cursor == 0);
    CHECK(is.kontor_counter == 13);
    CHECK(sim::find_figure(copy.figures(), sim::FigureId{1})->holds ==
          ship->holds);                                        // cargo untouched
    CHECK(copy.players().find(owner)->money == player->money); // gold untouched
    auto reloaded = sim::World::load(copy.save(), types);
    REQUIRE_MESSAGE(reloaded.ok(), (reloaded.ok() ? std::string() : reloaded.error()));
    CHECK(reloaded.value().state_hash() == copy.state_hash());
    CHECK(reloaded.value().check_invariants().empty());
    CHECK(reloaded.value().islands()[0]->city(sim::CityNo{0})->name == "Gamma");
    // The same owner cannot found a second city on this island, and the world is unchanged by the
    // refusal.
    const std::uint64_t after = copy.state_hash();
    auto again = sim::CityCreation::prepare(copy, rq);
    REQUIRE_FALSE(again.ok());
    CHECK(again.error() == sim::CityRefusal::OwnCityOnIsland);
    CHECK(copy.state_hash() == after);
}
