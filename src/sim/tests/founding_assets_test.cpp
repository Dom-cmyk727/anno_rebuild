// The founding command on the original files (task MAP-2 package 8d). The real registry
// (Anlagen.dat, FigurenHeader.dat), the real islands of the unchanged candidate lan5-ship-a through
// the ship-start loader, and the real STAEDTE name list. The legal site is FOUND by the engine's
// own site predicates, then the command is applied through World::apply; the candidate files are
// only read (compared before and after). Skips with a message without ANNO1503_GAME_DIR; the
// canonical gate sets it and treats a skip as a skip.
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/ship_manifest.hpp>
#include <anno/sim/command.hpp>
#include <anno/sim/site_checks.hpp>
#include <anno/sim/terrain_view.hpp>
#include <anno/sim/world.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/ship_start.hpp>
#include <anno/test/game_dir.hpp>

#include "world_access.hpp"

namespace sim = anno::sim;
namespace sl = anno::sim_load;
namespace sm = anno::formats::ship_manifest;
namespace site = anno::sim::site;
namespace fs = std::filesystem;

namespace {

std::vector<std::uint8_t> read_all(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

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

struct Site {
    std::size_t island = 0;
    std::int32_t click_x = 0, click_y = 0;
};

/// The first (island, click) in table and row-major order where the engine's predicates accept the
/// Kontor: some rotation inside the island, all footprint tiles neutral, compatible overlap and the
/// coast rule; the click that centres the 4 x 4 footprint on origin (ox, oy) is (ox + 1, oy + 1).
std::optional<Site> find_legal_site(const sim::World& w, const sim::BuildingType& type,
                                    std::uint32_t neutral) {
    const auto& islands = w.islands();
    std::optional<Site> first_legal;
    for (std::size_t i = 0; i < islands.size(); ++i) {
        if (!islands[i].has_value() || islands[i]->locked()) {
            continue;
        }
        const sim::Island& is = *islands[i];
        const sim::VertexGrid grid = sim::VertexGrid::of(is);
        if (!grid.ok()) {
            continue;
        }
        for (std::int64_t oy = is.tiles.y0() + 1;
             oy < std::int64_t{is.tiles.y0()} + is.tiles.height(); ++oy) {
            for (std::int64_t ox = is.tiles.x0() + 1;
                 ox < std::int64_t{is.tiles.x0()} + is.tiles.width(); ++ox) {
                const site::Origin o{ox, oy};
                const auto accepted = site::first_accepted_rotation(0, [&](std::uint8_t rot) {
                    const auto [fw, fh] = sim::footprint(type, rot);
                    return site::footprint_inside(is.tiles, o, fw, fh) &&
                           site::footprint_ownership(is.tiles, neutral, o, fw, fh) ==
                               site::Ownership::AllNeutral &&
                           site::footprint_occupancy(is.tiles, w.types(), type, o, fw, fh) ==
                               site::Compat::Compatible &&
                           site::coast_height_check(grid, type, rot, o).result ==
                               site::TerrainResult::Accepted;
                });
                if (accepted.has_value()) {
                    // Prefer a site with an ore object in claim-stage-2 reach (documented geometry,
                    // written out here: 4 x 4 centre origin + (1, 1), R = 24, radius-30 table), so
                    // the discovery check below is not vacuous; remember the first legal site.
                    static constexpr std::array<int, 25> kT30 = {30, 30, 30, 30, 30, 30, 29, 29, 29,
                                                                 29, 28, 28, 27, 27, 27, 26, 25, 25,
                                                                 24, 23, 22, 21, 20, 19, 18};
                    const std::int64_t cx = ox + 1, cy = oy + 1;
                    bool ore_in_reach = false;
                    for (const sim::OreObject& obj : is.ore_objects) {
                        const std::int64_t dy = obj.y > cy ? 1 + (obj.y - cy) : cy - obj.y;
                        const std::int64_t dx = obj.x > cx ? 1 + (obj.x - cx) : cx - obj.x;
                        ore_in_reach =
                            ore_in_reach || (dy <= 24 && dx <= kT30[static_cast<std::size_t>(dy)]);
                    }
                    const Site here{i, static_cast<std::int32_t>(ox + 1),
                                    static_cast<std::int32_t>(oy + 1)};
                    if (ore_in_reach) {
                        return here;
                    }
                    if (!first_legal.has_value()) {
                        first_legal = here;
                    }
                }
            }
        }
    }
    return first_legal;
}

} // namespace

TEST_CASE("founding on the original files: a legal site found by the engine, applied for real") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto types = original_types(*dir);
    // The registry data the contract's examples rest on (docs/sim/ship-founding.md 13.3).
    const sim::BuildingType* k = types->find(sim::TypeId{1302});
    REQUIRE(k != nullptr);
    CHECK(k->prodtype == 35);
    CHECK(k->width == 4);
    CHECK(k->height == 4);
    CHECK(k->flags_word == 0x2087);
    CHECK(k->cost_gold == 350);
    CHECK(k->action_radius == 24);
    REQUIRE(k->cost_goods.size() == 2);
    // Tools (0x29) 5 t and wood (0x2a) 12 t in 8.8.
    CHECK(k->cost_goods[0].goods == 0x29);
    CHECK(k->cost_goods[0].amount_88 == 1280);
    CHECK(k->cost_goods[1].goods == 0x2a);
    CHECK(k->cost_goods[1].amount_88 == 3072);
    // The figure facts of the three expedition types come from FigurenHeader.dat.
    const sim::FigureType* ship = types->find_figure_type(1502);
    const sim::FigureType* scout = types->find_figure_type(717);
    const sim::FigureType* donkey = types->find_figure_type(718);
    REQUIRE((ship != nullptr && scout != nullptr && donkey != nullptr));
    CHECK(ship->bewohntype == 113);
    CHECK(ship->range_88 == 16 * 256); // LOGIC[0x25] = 128 -> 16 tiles
    CHECK(ship->hull_half_88 == 397);  // HEAD f32 1.55 -> 396.8 -> 397
    CHECK(scout->bewohntype == 97);
    CHECK(scout->range_88 == 7 * 256); // LOGIC[0x25] = 56 -> 7 tiles
    CHECK(donkey->bewohntype == 53);
    CHECK_FALSE(scout->hull_half_88.has_value()); // a class without a hull
    // The ship-class types of the file: valid hulls, the present zero of 1509, the uninitialised
    // 0xCCCCCCCC fill of 391 kept as the signed float it is, and the absent field of 343 and 1510.
    const auto hull = [&](std::uint32_t id) { return types->find_figure_type(id)->hull_half_88; };
    CHECK(hull(1502) == std::optional<std::int64_t>(397));
    CHECK(hull(1501) == std::optional<std::int64_t>(602));
    CHECK(hull(1509) == std::optional<std::int64_t>(0));
    CHECK(hull(391) == std::optional<std::int64_t>(-27487789056LL));
    CHECK_FALSE(hull(343).has_value());
    CHECK_FALSE(hull(1510).has_value());

    const fs::path manifest_path = fs::path(ANNO_REPO_DIR) / "data" / "maps" / "candidates" /
                                   "ship-start-v2" / "lan5-ship-a.json";
    const std::vector<std::uint8_t> manifest_bytes = read_all(manifest_path);
    const auto parsed = sm::parse(manifest_bytes);
    REQUIRE(parsed.ok());
    const auto names = sl::load_city_names(*dir);
    REQUIRE_MESSAGE(names.ok(), (names.ok() ? std::string() : names.error()));
    REQUIRE(!names.value().empty());
    sim::Rules rules;
    rules.city_names = names.value();
    auto loaded = sl::load_ship_start(parsed.value(), *dir, types, {}, rules);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    sim::World w = std::move(loaded).value();
    REQUIRE(w.check_invariants().empty());
    const std::uint64_t pristine_hash = w.state_hash();

    const std::optional<Site> found = find_legal_site(w, *k, w.config().neutral_owner);
    REQUIRE_MESSAGE(found.has_value(), "no island of the candidate has a legal 1302 site");
    INFO("island " << found->island << " click (" << found->click_x << ", " << found->click_y
                   << ")");
    const sim::Island& is = *w.islands()[found->island];
    CHECK(is.cities.size() == w.config().city_slots);
    for (const auto& c : is.cities) {
        CHECK_FALSE(c.has_value()); // the ship start has no city
    }

    // The seat-0 ship is placed directly beside the site: a position on the click row 5 tiles
    // away in one of four directions that passes every check (the sea side). Each try runs on a
    // copy; the first accepted offset is then applied to the real world.
    const sim::Figure* ship0 = sim::find_figure(w.figures(), sim::FigureId{1});
    REQUIRE(ship0 != nullptr);
    sim::FoundCmd cmd;
    cmd.ship = sim::FigureId{1};
    cmd.placement.type = sim::TypeId{1302};
    cmd.placement.rotation = 0;
    cmd.placement.x = found->click_x - 1;
    cmd.placement.y = found->click_y - 1;
    cmd.click_x = found->click_x;
    cmd.click_y = found->click_y;
    const sim::Command command{sim::PlayerId{0}, cmd};
    bool placed = false;
    for (const std::pair<int, int> d : {std::pair{6, 0}, std::pair{-6, 0}, std::pair{0, 6},
                                        std::pair{0, -6}, std::pair{7, 7}, std::pair{-7, -7}}) {
        sim::World trial = w;
        sim::Figure* s = sim::find_figure(sim::WorldTestAccess::figures(trial), sim::FigureId{1});
        s->pos.x = (found->click_x + d.first) * 256 + 128;
        s->pos.y = (found->click_y + d.second) * 256 + 128;
        if (trial.apply(command).ok()) {
            sim::Figure* real =
                sim::find_figure(sim::WorldTestAccess::figures(w), sim::FigureId{1});
            real->pos = s->pos;
            placed = true;
            break;
        }
    }
    REQUIRE_MESSAGE(placed, "no ship position within range passed the command at the found site");

    const std::int64_t gold_before = w.players().find(sim::PlayerId{0})->money;
    const std::vector<sim::ShipHold> holds_before =
        sim::find_figure(w.figures(), sim::FigureId{1})->holds;
    const std::uint32_t cursor_before = w.rng().cursor();
    const sim::ApplyResult r = w.apply(command);
    REQUIRE(r.ok());
    // Payment from the baseline loadout (gold 30000; wood, tools in the holds), exactly the
    // contract's: 350 gold, tools 1280 raw from the first tools hold, wood 3072 from the wood hold.
    CHECK(gold_before == 30000);
    CHECK(w.players().find(sim::PlayerId{0})->money == 29650);
    const std::vector<sim::ShipHold>& holds_after =
        sim::find_figure(w.figures(), sim::FigureId{1})->holds;
    REQUIRE(holds_after.size() == holds_before.size());
    std::uint64_t taken_wood = 0, taken_tools = 0;
    for (std::size_t i = 0; i < holds_before.size(); ++i) {
        CHECK(holds_after[i].goods == holds_before[i].goods);
        const std::uint64_t d = holds_before[i].amount_88 - holds_after[i].amount_88;
        if (holds_before[i].goods == 0x2a) {
            taken_wood += d;
        } else if (holds_before[i].goods == 0x29) {
            taken_tools += d;
        } else {
            CHECK(d == 0);
        }
    }
    CHECK(taken_wood == 3072);
    CHECK(taken_tools == 1280);
    // The city, the building and the single name draw with the real list.
    const sim::Island& after = *w.islands()[found->island];
    REQUIRE(after.cities[0].has_value());
    CHECK(after.cities[0]->owner == sim::PlayerId{0});
    CHECK_FALSE(after.cities[0]->name.empty());
    CHECK(w.rng().cursor() == cursor_before + 1);
    const sim::Building* b = after.find_building(r.records[0].building);
    REQUIRE(b != nullptr);
    CHECK(b->type == sim::TypeId{1302});
    CHECK(b->byte0f == 0x30);
    CHECK(b->kontor_id == 1);
    CHECK(after.kontor_counter == 2);
    std::uint64_t owned = 0;
    for (std::uint64_t i = 0; i < after.tiles.tile_count(); ++i) {
        owned += after.tiles.owner(i) == 0 ? 1 : 0;
    }
    CHECK(owned >= 16);
    CHECK(owned <= 1976); // the disc of radius 24, clipped by the island
    CHECK(w.check_invariants().empty());
    CHECK(w.state_hash() != pristine_hash);
    // Claim stage 2 on the real islands (ore objects loaded from OCCUR by the ship-start loader):
    // for each ore object of the island the founder is in its discovery set exactly when the
    // documented geometry says so, computed here independently (4 x 4 footprint: centre = origin +
    // (1, 1), parity term 1 on the south and east side, R = 24, the radius-30 table).
    {
        const std::array<int, 25> t30 = {30, 30, 30, 30, 30, 30, 29, 29, 29, 29, 28, 28, 27,
                                         27, 27, 26, 25, 25, 24, 23, 22, 21, 20, 19, 18};
        std::size_t total_ore = 0;
        for (const auto& island_slot : w.islands()) {
            total_ore += island_slot.has_value() ? island_slot->ore_objects.size() : 0;
        }
        CHECK(total_ore > 0); // the loader did bring ore objects into the world
        const int cx = b->x + 1, cy = b->y + 1;
        std::size_t in_reach = 0;
        for (const sim::OreObject& o : after.ore_objects) {
            const int dy = o.y > cy ? 1 + (o.y - cy) : cy - o.y;
            const int dx = o.x > cx ? 1 + (o.x - cx) : cx - o.x;
            const bool want = dy <= 24 && dx <= t30[static_cast<std::size_t>(dy)];
            in_reach += want ? 1 : 0;
            const bool has =
                std::binary_search(o.discovered.begin(), o.discovered.end(), sim::PlayerId{0});
            CHECK(has == want);
        }
        CHECK(in_reach > 0); // the chosen site has ore in reach: the equality above is not vacuous
        MESSAGE("founding on the original files: " << after.ore_objects.size()
                                                   << " ore objects on the island, " << in_reach
                                                   << " in reach");
    }
    // The candidate file is unchanged by all of this.
    CHECK(read_all(manifest_path) == manifest_bytes);

    // A load of the same start in a fresh world and the same command gives the same hash
    // (determinism of the whole path on the real data).
    auto again = sl::load_ship_start(parsed.value(), *dir, types, {}, rules);
    REQUIRE(again.ok());
    sim::World w2 = std::move(again).value();
    sim::find_figure(sim::WorldTestAccess::figures(w2), sim::FigureId{1})->pos =
        sim::find_figure(w.figures(), sim::FigureId{1})->pos;
    REQUIRE(w2.apply(command).ok());
    CHECK(w2.state_hash() == w.state_hash());
}
