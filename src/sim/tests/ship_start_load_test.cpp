// The ship-start loader (task MAP-2; docs/sim/ship-start-integration.md §5).
// Synthetic cases use fixtures/synthetic/fixed_map_game; the candidate cases read the
// original game files (ANNO1503_GAME_DIR) and skip with a message when it is unset.
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/ship_manifest.hpp>
#include <anno/sim/executor.hpp>
#include <anno/sim/world.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/ship_start.hpp>
#include <anno/test/game_dir.hpp>

#include "island_helpers.hpp"
#include "ship_helpers.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
namespace sl = anno::sim_load;
namespace sm = anno::formats::ship_manifest;
namespace fs = std::filesystem;
using ship_test::goods;
using ship_test::ship_registry;

namespace {

fs::path synthetic_game() {
    return fs::path(ANNO_FIXTURES_DIR) / "fixed_map_game";
}

/// A baseline manifest over the synthetic game directory: edge 128, two islands, five seats
/// at tile centres (t, 100) for t = 10, 30, 50, 70, 90, facings 0, 1, 2, 3, 0.
sm::Manifest synthetic_manifest() {
    sm::Manifest m;
    m.name = "synthetic";
    m.edge_w = m.edge_h = 128;
    m.base = "Scenes/base.sze";
    m.seed = 42;
    m.generator_tool = "test";
    m.generator_config = "test";
    m.islands = {{0, 0, 0, "NORD/N_A.SCP"}, {1, 64, 0, "DSCHUNGEL/D_A.SCP"}};
    for (std::int64_t seat = 0; seat < 5; ++seat) {
        m.spawns.push_back({seat, 1000 * (10 + 20 * seat) + 500, 1000 * 100 + 500, seat % 4});
    }
    m.loadout = sm::baseline_loadout();
    return m;
}

std::string message_of(const anno::formats::Result<sim::World, std::string>& r) {
    return r.ok() ? std::string() : r.error();
}

} // namespace

TEST_CASE("ship start: milli-tiles convert exactly or not at all") {
    const auto fixed = [](std::int64_t m) { return sl::milli_tile_to_fixed(m); };
    CHECK(fixed(500).value() == 128);
    CHECK(fixed(110500).value() == 28288); // 256 * 110 + 128
    CHECK(fixed(-125).value() == -32);
    CHECK(fixed(0).value() == 0);
    CHECK(fixed(125).value() == 32);
    // Every tile centre 1000 t + 500 is exact and is 256 t + 128.
    for (std::int64_t t = 0; t < 2000; ++t) {
        const auto q = fixed(1000 * t + 500);
        REQUIRE(q.ok());
        CHECK(q.value() == 256 * t + 128);
    }
    // Not divisible by 125 after the scaling by 32: rejected, never rounded.
    for (const std::int64_t bad : {1, 2, 124, 126, 501, -1, 999}) {
        CAPTURE(bad);
        CHECK_FALSE(fixed(bad).ok());
    }
    // The int32 range: 2^31 - 1 is 2147483647; the largest exact milli is 125 * 2^26 - ...
    const std::int64_t max_ok = 125LL * 67108863LL; // q = 67108863 * 32 = 2147483616
    CHECK(fixed(max_ok).value() == 2147483616);
    CHECK_FALSE(fixed(125LL * 67108864LL).ok()); // q = 2^31
    CHECK_FALSE(fixed(INT64_MAX).ok());
    CHECK_FALSE(fixed(INT64_MIN).ok());
}

TEST_CASE("ship start: facing 0, 1, 2, 3 is dir16 12, 8, 4, 0") {
    CHECK(sl::facing_to_dir16(0) == std::optional<std::uint8_t>(12));
    CHECK(sl::facing_to_dir16(1) == std::optional<std::uint8_t>(8));
    CHECK(sl::facing_to_dir16(2) == std::optional<std::uint8_t>(4));
    CHECK(sl::facing_to_dir16(3) == std::optional<std::uint8_t>(0));
    CHECK_FALSE(sl::facing_to_dir16(4).has_value());
    CHECK_FALSE(sl::facing_to_dir16(-1).has_value());
}

TEST_CASE("ship start: a baseline manifest loads the hand-derived world") {
    const auto types = ship_registry();
    const sm::Manifest m = synthetic_manifest();
    auto loaded = sl::load_ship_start(m, synthetic_game(), types);
    REQUIRE_MESSAGE(loaded.ok(), message_of(loaded));
    const sim::World& w = loaded.value();
    CHECK(w.check_invariants().empty());
    CHECK(sl::expedition_baseline_problem(w).empty());
    // Five players, 30000 gold each, in slots 0..4 and no neutral player invented.
    for (std::uint32_t seat = 0; seat < 5; ++seat) {
        REQUIRE(w.players().find(sim::PlayerId{seat}) != nullptr);
        CHECK(w.players().find(sim::PlayerId{seat})->money == 30000);
    }
    CHECK(w.ids().next_figure == 16);
    CHECK(w.figures().by_id.size() == 15);
    // Seat 0: ids 1, 2, 3; seat 4: 13, 14, 15. Seat s stands at tile centre (10 + 20 s, 100).
    const std::int32_t dir16_of_seat[5] = {12, 8, 4, 0, 12};
    for (std::uint32_t seat = 0; seat < 5; ++seat) {
        for (std::uint32_t k = 0; k < 3; ++k) {
            const sim::Figure* f = sim::find_figure(w.figures(), sim::FigureId{3 * seat + 1 + k});
            REQUIRE(f != nullptr);
            CHECK(f->pos.x == static_cast<std::int32_t>(256 * (10 + 20 * seat) + 128));
            CHECK(f->pos.y == 256 * 100 + 128);
            CHECK(f->dir16 == dir16_of_seat[seat]);
        }
    }
    // No city, no owned tile, no developer start; the islands are in.
    std::size_t islands = 0;
    for (const auto& island : w.islands()) {
        if (!island) {
            continue;
        }
        ++islands;
        for (const auto& city : island->cities) {
            CHECK_FALSE(city.has_value());
        }
        for (std::uint64_t t = 0; t < island->tiles.tile_count(); ++t) {
            CHECK(island->tiles.owner(t) == w.config().neutral_owner);
        }
    }
    CHECK(islands == 2);
    // The ship's holds in file order, the two tools entries unmerged.
    const sim::Figure* ship = sim::find_figure(w.figures(), sim::FigureId{1});
    REQUIRE(ship != nullptr);
    REQUIRE(ship->holds.size() == 4);
    CHECK(ship->holds[2] == sim::ShipHold{goods(*types, "WERKZEUG"), 50});
    CHECK(ship->holds[3] == sim::ShipHold{goods(*types, "WERKZEUG"), 30});
    // The generator was not drawn from.
    CHECK(w.rng().cursor() == 42 % w.rng().size());
}

TEST_CASE("ship start: the baseline expectation fails a donkey parented to the ship") {
    const auto types = ship_registry();
    auto loaded = sl::load_ship_start(synthetic_manifest(), synthetic_game(), types);
    REQUIRE(loaded.ok());
    sim::World w = std::move(loaded).value();
    REQUIRE(sl::expedition_baseline_problem(w).empty());
    sim::Figure* donkey = sim::find_figure(sim::WorldTestAccess::figures(w), sim::FigureId{3});
    REQUIRE(donkey != nullptr);
    donkey->parent = sim::FigureId{1}; // the ship
    donkey->crew_slot = 1;             // a valid graph (crew slots 0 and 1 of the ship)...
    CHECK(w.check_invariants().empty());
    const std::string problem = sl::expedition_baseline_problem(w);
    CHECK_FALSE(problem.empty()); // ...but not the file's tree
    CHECK(problem.find("donkey") != std::string::npos);
}

TEST_CASE(
    "ship start: initialisation is deterministic, also across executors and processes' state") {
    const auto types = ship_registry();
    const sm::Manifest m = synthetic_manifest();
    const std::uint64_t first =
        sl::load_ship_start(m, synthetic_game(), types).value().state_hash();
    CHECK(sl::load_ship_start(m, synthetic_game(), types).value().state_hash() == first);
    for (const unsigned threads : {1U, 2U, 5U}) {
        island_test::ThreadExecutor ex(threads);
        CAPTURE(threads);
        CHECK(sl::load_ship_start(m, synthetic_game(), types, {}, {}, ex).value().state_hash() ==
              first);
    }
    // The save round trip keeps every new field.
    const sim::World w = sl::load_ship_start(m, synthetic_game(), types).value();
    const std::vector<std::uint8_t> bytes = w.save();
    auto again = sim::World::load(bytes, types);
    REQUIRE_MESSAGE(again.ok(), (again.ok() ? std::string() : again.error()));
    CHECK(again.value().state_hash() == first);
    CHECK(again.value().save() == bytes);
    CHECK(sl::expedition_baseline_problem(again.value()).empty());
}

TEST_CASE("ship start: an idle advance leaves the loaded expedition alone") {
    const auto types = ship_registry();
    sim::World w = sl::load_ship_start(synthetic_manifest(), synthetic_game(), types).value();
    for (int i = 0; i < 1000; ++i) {
        w.advance(50);
    }
    CHECK(w.check_invariants().empty());
    CHECK(sl::expedition_baseline_problem(w).empty());
    CHECK(w.ids().next_figure == 16);
}

TEST_CASE("ship start: invalid input is refused with the offending field and nothing changes") {
    const auto types = ship_registry();
    // The world that must survive every refusal.
    sim::World live = sl::load_ship_start(synthetic_manifest(), synthetic_game(), types).value();
    const std::uint64_t before = live.state_hash();
    const std::vector<std::uint8_t> saved = live.save();

    const auto refuse = [&](auto mutate, const char* field) {
        sm::Manifest m = synthetic_manifest();
        mutate(m);
        CAPTURE(std::string(field));
        const auto direct = sl::load_ship_start(m, synthetic_game(), types);
        REQUIRE_FALSE(direct.ok());
        CHECK_MESSAGE(direct.error().find(field) != std::string::npos, direct.error());
        const auto installed = sl::install_ship_start(live, m, synthetic_game(), types);
        REQUIRE(installed.has_value());
        CHECK(live.state_hash() == before);
        CHECK(live.save() == saved);
    };
    refuse([](sm::Manifest& m) { m.loadout.gold = 25000; }, "loadout");
    refuse([](sm::Manifest& m) { m.loadout.holds.pop_back(); }, "loadout");
    refuse([](sm::Manifest& m) { m.spawns[2].x = 1; }, "spawns[2].x");
    refuse([](sm::Manifest& m) { m.spawns[3].y = 100001; }, "spawns[3].y");
    refuse([](sm::Manifest& m) { m.spawns[1].facing = 4; }, "spawns[1].facing");
    refuse([](sm::Manifest& m) { m.spawns[4].x = 1000 * 200 + 500; }, "spawns[4]");
    refuse([](sm::Manifest& m) { m.spawns.pop_back(); }, "spawns");
    refuse([](sm::Manifest& m) { std::swap(m.spawns[0], m.spawns[1]); }, "spawns[0].seat");
    refuse([](sm::Manifest& m) { m.islands[1].key = "NORD/NO_SUCH.SCP"; }, "islands[1].key");
    refuse([](sm::Manifest& m) { m.islands[1].x = 100; }, "islands[1]"); // beyond the edge
    refuse([](sm::Manifest& m) { m.islands[1].x = 0; }, "overlaps");
    refuse([](sm::Manifest& m) { m.base = "Scenes/missing.sze"; }, "base");
    refuse([](sm::Manifest& m) { m.islands[0].id = 100000; }, "islands[0].id");
}

TEST_CASE("ship start: content the registry lacks is refused") {
    // A registry without the donkey's figure type: the load names the field.
    auto base = anno::sim::synthetic::make_synthetic_types();
    REQUIRE(base);
    const auto r =
        sl::load_ship_start(synthetic_manifest(), synthetic_game(), base.value().registry);
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().find("loadout.ship_figure") != std::string::npos);
    CHECK_FALSE(sl::manifest_goods(*base.value().registry, "gold").has_value());
    CHECK(sl::manifest_goods(*base.value().registry, "wood").has_value());
}

// ---- the original game files

namespace {

std::shared_ptr<const sim::TypeRegistry> original_types(const fs::path& game) {
    static std::shared_ptr<const sim::TypeRegistry> cached;
    if (!cached) {
        const sl::OriginalRegistryLoad loaded =
            sl::load_original_registry(game, sl::FigureEntries::Required);
        REQUIRE_MESSAGE(loaded.error.empty(), loaded.error);
        cached = loaded.types;
    }
    return cached;
}

std::vector<std::uint8_t> read_all(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE_MESSAGE(in, path.string());
    return {std::istreambuf_iterator<char>(in), {}};
}

} // namespace

TEST_CASE("ship start: the three unchanged candidates load against the original files") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto types = original_types(*dir);
    // The original goods ids the manifest names (docs/research/endless-start-conditions.md).
    CHECK(sl::manifest_goods(*types, "food") == std::optional<sim::GoodsId>(26));
    CHECK(sl::manifest_goods(*types, "spices") == std::optional<sim::GoodsId>(31));
    CHECK(sl::manifest_goods(*types, "tools") == std::optional<sim::GoodsId>(41));
    CHECK(sl::manifest_goods(*types, "wood") == std::optional<sim::GoodsId>(42));
    CHECK(sl::manifest_goods(*types, "bricks") == std::optional<sim::GoodsId>(43));
    for (const unsigned kind : {1502U, 717U, 718U}) {
        CHECK(types->find_figure_type(kind) != nullptr);
    }
    const fs::path candidates =
        fs::path(ANNO_REPO_DIR) / "data" / "maps" / "candidates" / "ship-start-v2";
    for (const char* name : {"lan5-ship-a.json", "lan5-ship-b.json", "lan5-ship-c.json"}) {
        CAPTURE(name);
        const auto bytes = read_all(candidates / name);
        const auto parsed = sm::parse(bytes);
        REQUIRE(parsed.ok());
        auto loaded = sl::load_ship_start(parsed.value(), *dir, types);
        REQUIRE_MESSAGE(loaded.ok(), message_of(loaded));
        const sim::World& w = loaded.value();
        CHECK(w.check_invariants().empty());
        CHECK(sl::expedition_baseline_problem(w).empty());
        CHECK(w.settings().world_width == 1431);
        CHECK(w.islands().size() > 20);
        // Each spawn centre 1000 t + 500 became 256 t + 128 in the ship.
        for (std::uint32_t seat = 0; seat < 5; ++seat) {
            const sim::Figure* ship = sim::find_figure(w.figures(), sim::FigureId{3 * seat + 1});
            REQUIRE(ship != nullptr);
            const sm::Spawn& s = parsed.value().spawns[seat];
            CHECK(ship->pos.x == static_cast<std::int32_t>(s.x * 32 / 125));
            CHECK(ship->pos.y == static_cast<std::int32_t>(s.y * 32 / 125));
        }
        // A second fresh load in this process is bit-identical.
        CHECK(sl::load_ship_start(parsed.value(), *dir, types).value().state_hash() ==
              w.state_hash());
    }
    // The candidate files are not rewritten and not selected: reading them did not touch them
    // (the hash of the active map is checked by the task's gate, not here).
}
