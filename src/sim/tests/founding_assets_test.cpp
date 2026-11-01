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
#include <anno/sim/figure.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/scenario_formats.hpp>
#include <anno/sim/site_checks.hpp>
#include <anno/sim/terrain_view.hpp>
#include <anno/sim/world.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/ship_start.hpp>
#include <anno/test/game_dir.hpp>

#include "world_access.hpp"

namespace {
// Test-setup relocation (not a sailed route): moves the figure and keeps the navigation
// motion state in step, so the invariant Figure::pos == floor(pos_u) still holds.
void relocate(anno::sim::Figure& f, anno::sim::FixedPos pos) {
    f.pos = pos;
    if (f.nav) {
        f.nav->pos_u[0] = std::int64_t{pos.x} * anno::sim::kNavPosUnitsPerQ8;
        f.nav->pos_u[1] = std::int64_t{pos.y} * anno::sim::kNavPosUnitsPerQ8;
        f.nav->res24[0] = f.nav->res24[1] = 0;
    }
}
} // namespace

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
    // The navigation facts of the file (MAP-NAV-2 B): 1502 has HEAD f0 0.11, f1 0.036, LOGIC holds
    // 4/2, class 0, 6000 hp, stamp byte 40; the same conversions as the synthetic case, now from
    // the shipped bytes. 343 and 1510 (no hit points) and the scout and donkey do not navigate.
    REQUIRE(ship->nav.has_value());
    CHECK(ship->nav->speed_q32 == 154618816U);
    CHECK(ship->nav->turn_step_q32 == 472446400U);
    CHECK(ship->nav->max_hp == 6000U);
    CHECK(ship->nav->hold_count_a == 4U);
    CHECK(ship->nav->hold_count_b == 2U);
    CHECK(ship->nav->nav_class == 0U);
    CHECK(ship->nav->stamp_radius_88 == 1280U);
    CHECK_FALSE(types->find_figure_type(343)->nav.has_value());
    CHECK_FALSE(types->find_figure_type(1510)->nav.has_value());
    CHECK_FALSE(scout->nav.has_value());
    CHECK_FALSE(donkey->nav.has_value());

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
    REQUIRE_MESSAGE(w.check_invariants().empty(), w.check_invariants());
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
            relocate(*real, s->pos);
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
    CHECK_MESSAGE(w.check_invariants().empty(), w.check_invariants());
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
            // The frame guard: the loaded position is inside the island (OCCUR floats are
            // island-local, the loader added the origin).
            CHECK(o.x >= after.tiles.x0());
            CHECK(o.y >= after.tiles.y0());
            CHECK(o.x < std::int64_t{after.tiles.x0()} + after.tiles.width());
            CHECK(o.y < std::int64_t{after.tiles.y0()} + after.tiles.height());
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
    relocate(*sim::find_figure(sim::WorldTestAccess::figures(w2), sim::FigureId{1}),
             sim::find_figure(w.figures(), sim::FigureId{1})->pos);
    REQUIRE(w2.apply(command).ok());
    CHECK(w2.state_hash() == w.state_hash());
}

// ---- MAP-FOUND-3 package D on the real loader (docs/sim/shared-visibility.md) ------------------

namespace {

struct RealStart {
    sm::Manifest manifest;
    std::vector<std::uint8_t> manifest_bytes;
    std::shared_ptr<const sim::TypeRegistry> types;
    fs::path dir;
    sim::Rules rules;
};

RealStart real_start(const fs::path& dir) {
    RealStart r;
    r.dir = dir;
    r.types = original_types(dir);
    const fs::path manifest_path = fs::path(ANNO_REPO_DIR) / "data" / "maps" / "candidates" /
                                   "ship-start-v2" / "lan5-ship-a.json";
    r.manifest_bytes = read_all(manifest_path);
    auto parsed = sm::parse(r.manifest_bytes);
    REQUIRE(parsed.ok());
    r.manifest = std::move(parsed).value();
    auto names = sl::load_city_names(dir);
    REQUIRE(names.ok());
    r.rules.city_names = names.value();
    return r;
}

std::vector<sim::PlayerId> pid_set(std::initializer_list<std::uint32_t> v) {
    std::vector<sim::PlayerId> out;
    for (const std::uint32_t i : v) {
        out.push_back(sim::PlayerId{i});
    }
    return out;
}

/// The seat's ship is placed beside `site` (the first offset that passes, tried on copies) and the
/// founding command is returned; nullopt when no offset passes.
std::optional<sim::Command> place_for_founding(sim::World& w, const Site& site,
                                               std::uint32_t seat) {
    sim::FoundCmd cmd;
    cmd.ship = sim::FigureId{3ULL * seat + 1};
    cmd.placement.type = sim::TypeId{1302};
    cmd.placement.rotation = 0;
    cmd.placement.x = site.click_x - 1;
    cmd.placement.y = site.click_y - 1;
    cmd.click_x = site.click_x;
    cmd.click_y = site.click_y;
    const sim::Command command{sim::PlayerId{seat}, cmd};
    for (const std::pair<int, int> d : {std::pair{6, 0}, std::pair{-6, 0}, std::pair{0, 6},
                                        std::pair{0, -6}, std::pair{7, 7}, std::pair{-7, -7}}) {
        sim::World trial = w;
        sim::Figure* s = sim::find_figure(sim::WorldTestAccess::figures(trial), cmd.ship);
        s->pos.x = (site.click_x + d.first) * 256 + 128;
        s->pos.y = (site.click_y + d.second) * 256 + 128;
        if (trial.apply(command).ok()) {
            relocate(*sim::find_figure(sim::WorldTestAccess::figures(w), cmd.ship), s->pos);
            return command;
        }
    }
    return std::nullopt;
}

bool in_reach_of_origin(const sim::OreObject& o, std::int64_t origin_x, std::int64_t origin_y) {
    static constexpr std::array<int, 25> kT30 = {30, 30, 30, 30, 30, 30, 29, 29, 29, 29, 28, 28, 27,
                                                 27, 27, 26, 25, 25, 24, 23, 22, 21, 20, 19, 18};
    const std::int64_t cx = origin_x + 1, cy = origin_y + 1;
    const std::int64_t dy = o.y > cy ? 1 + (o.y - cy) : cy - o.y;
    const std::int64_t dx = o.x > cx ? 1 + (o.x - cx) : cx - o.x;
    return dy <= 24 && dx <= kT30[static_cast<std::size_t>(dy)];
}

} // namespace

TEST_CASE("ship start policy: peace, shared visibility and no alliances on the real loader") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    RealStart rs = real_start(*dir);
    const sim::BuildingType& kontor = *rs.types->find(sim::TypeId{1302});

    // ---- the loaded start: the owner default (all five seats) at peace, nothing revealed -------
    auto loaded = sl::load_ship_start(rs.manifest, *dir, rs.types, {}, rs.rules);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    sim::World w = std::move(loaded).value();
    REQUIRE_MESSAGE(w.check_invariants().empty(), w.check_invariants());
    CHECK(w.rules().shared_visibility == std::vector<std::vector<std::uint32_t>>{{0, 1, 2, 3, 4}});
    for (std::uint32_t a = 0; a < 5; ++a) {
        const sim::Player* pa = w.players().find(sim::PlayerId{a});
        REQUIRE(pa != nullptr);
        CHECK(pa->relations.empty()); // no DIPL row at all, not merely zero values
        CHECK(pa->discovery.empty()); // unexplored stays unknown
        for (std::uint32_t b = 0; b < 5; ++b) {
            for (std::uint32_t kind = 0; kind < 3; ++kind) {
                CHECK(pa->relation(kind, sim::PlayerId{b}) == 0); // peace: no trade/military/war
            }
        }
    }
    std::size_t ore_total = 0;
    for (const auto& island : w.islands()) {
        if (!island) {
            continue;
        }
        CHECK(island->players_present.empty());
        for (const sim::OreObject& o : island->ore_objects) {
            CHECK(o.discovered.empty());
            ++ore_total;
        }
        for (std::uint64_t i = 0; i < island->tiles.tile_count(); ++i) {
            REQUIRE(island->tiles.owner(i) == w.config().neutral_owner); // no territorial right
        }
    }
    CHECK(ore_total > 0);
    const std::uint64_t pristine = w.state_hash();

    // ---- a founding by seat 0 reaches all five seats, and nothing else ------------------------
    const std::optional<Site> site = find_legal_site(w, kontor, w.config().neutral_owner);
    REQUIRE(site.has_value());
    const auto command = place_for_founding(w, *site, 0);
    REQUIRE(command.has_value());
    const sim::World before_founding = w;
    const sim::ApplyResult r = w.apply(*command);
    REQUIRE(r.ok());
    REQUIRE_MESSAGE(w.check_invariants().empty(), w.check_invariants());
    const sim::Island& is = *w.islands()[site->island];
    REQUIRE(is.cities[0].has_value());
    CHECK(is.cities[0]->player_mask == pid_set({0, 1, 2, 3, 4}));
    CHECK(is.players_present == pid_set({0, 1, 2, 3, 4}));
    const sim::Building* b = is.find_building(r.records[0].building);
    REQUIRE(b != nullptr);
    std::size_t reached = 0;
    for (const sim::OreObject& o : is.ore_objects) {
        const bool want = in_reach_of_origin(o, b->x, b->y);
        reached += want ? 1 : 0;
        CHECK(o.discovered == (want ? pid_set({0, 1, 2, 3, 4}) : pid_set({})));
    }
    CHECK(reached > 0); // not vacuous
    for (std::uint32_t p = 0; p < 5; ++p) {
        // The recount: every player in the new city's mask learns its owner (seat 0).
        CHECK(w.players().find(sim::PlayerId{p})->discovery == pid_set({0}));
        CHECK(w.players().find(sim::PlayerId{p})->relations.empty()); // still no alliance
        CHECK(w.players().find(sim::PlayerId{p})->money ==
              (p == 0 ? 29650 : before_founding.players().find(sim::PlayerId{p})->money));
    }
    // Other islands stay unexplored; the only owned tiles are the founder's city 0 on this island.
    for (std::size_t i = 0; i < w.islands().size(); ++i) {
        if (!w.islands()[i] || i == site->island) {
            continue;
        }
        CHECK(w.islands()[i]->players_present.empty());
        for (const sim::OreObject& o : w.islands()[i]->ore_objects) {
            CHECK(o.discovered.empty());
        }
    }
    for (std::uint64_t i = 0; i < is.tiles.tile_count(); ++i) {
        const std::uint32_t owner = is.tiles.owner(i);
        CHECK((owner == w.config().neutral_owner || owner == 0));
    }
    // Save/load and replay on the real data.
    auto reloaded = sim::World::load(w.save(), rs.types, {});
    REQUIRE_MESSAGE(reloaded.ok(), (reloaded.ok() ? std::string() : reloaded.error()));
    CHECK(reloaded.value().state_hash() == w.state_hash());
    CHECK(reloaded.value().rules().shared_visibility == w.rules().shared_visibility);
    {
        auto fresh = sl::load_ship_start(rs.manifest, *dir, rs.types, {}, rs.rules);
        REQUIRE(fresh.ok());
        sim::World w3 = std::move(fresh).value();
        relocate(*sim::find_figure(sim::WorldTestAccess::figures(w3), sim::FigureId{1}),
                 sim::find_figure(w.figures(), sim::FigureId{1})->pos);
        auto started = sim::Recorder::start(w3);
        REQUIRE(started.ok());
        sim::Recorder rec = std::move(started).value();
        REQUIRE(rec.submit(*command).ok());
        rec.advance(1000);
        rec.checkpoint();
        auto again = sl::load_ship_start(rs.manifest, *dir, rs.types, {}, rs.rules);
        REQUIRE(again.ok());
        sim::World w4 = std::move(again).value();
        relocate(*sim::find_figure(sim::WorldTestAccess::figures(w4), sim::FigureId{1}),
                 sim::find_figure(w.figures(), sim::FigureId{1})->pos);
        auto outcome = sim::replay(w4, rec.log());
        REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
        CHECK(outcome.value().reproduced());
        CHECK(w4.state_hash() == w3.state_hash());
    }

    // ---- the policy is gameplay setup, not geometry -------------------------------------------
    auto none =
        sl::load_ship_start(rs.manifest, *dir, rs.types, {}, rs.rules, sim::serial_executor(),
                            sl::ShipStartPolicy{std::vector<std::uint32_t>{}});
    REQUIRE_MESSAGE(none.ok(), (none.ok() ? std::string() : none.error()));
    CHECK(none.value().rules().shared_visibility.empty());
    CHECK(none.value().state_hash() != pristine); // different gameplay setup identity
    CHECK(none.value().rules().digest() != w.rules().digest());
    CHECK(read_all(fs::path(ANNO_REPO_DIR) / "data" / "maps" / "candidates" / "ship-start-v2" /
                   "lan5-ship-a.json") ==
          rs.manifest_bytes); // the candidate geometry file is unchanged
    CHECK(sm::parse(rs.manifest_bytes).value().content_hash == rs.manifest.content_hash);
}

TEST_CASE("ship start policy: other participant sets, non-default seats, refused inputs") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    RealStart rs = real_start(*dir);
    const sim::BuildingType& kontor = *rs.types->find(sim::TypeId{1302});
    // Participants {1, 3, 4}: seat 3 founds; 1 and 4 share, 0 and 2 (existing seats) do not.
    auto loaded =
        sl::load_ship_start(rs.manifest, *dir, rs.types, {}, rs.rules, sim::serial_executor(),
                            sl::ShipStartPolicy{std::vector<std::uint32_t>{1, 3, 4}});
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    sim::World w = std::move(loaded).value();
    CHECK(w.rules().shared_visibility == std::vector<std::vector<std::uint32_t>>{{1, 3, 4}});
    const std::optional<Site> site = find_legal_site(w, kontor, w.config().neutral_owner);
    REQUIRE(site.has_value());
    const auto command = place_for_founding(w, *site, 3);
    REQUIRE(command.has_value());
    REQUIRE(w.apply(*command).ok());
    const sim::Island& is = *w.islands()[site->island];
    CHECK(is.cities[0]->owner == sim::PlayerId{3});
    CHECK(is.cities[0]->player_mask == pid_set({1, 3, 4}));
    CHECK(is.players_present == pid_set({1, 3, 4}));
    for (const std::uint32_t p : {0U, 2U}) {
        CHECK(w.players().find(sim::PlayerId{p})->discovery.empty());
    }
    for (const std::uint32_t p : {1U, 3U, 4U}) {
        CHECK(w.players().find(sim::PlayerId{p})->discovery == pid_set({3}));
    }
    // Refused inputs: a non-seat, a one-member group, two sources of the policy; each a clear
    // error and no world.
    const auto error_of = [&](const sl::ShipStartPolicy& policy, const sim::Rules& rules) {
        auto r = sl::load_ship_start(rs.manifest, *dir, rs.types, {}, rules, sim::serial_executor(),
                                     policy);
        return r.ok() ? std::string() : r.error();
    };
    const auto has = [](const std::string& e, const char* part) {
        return e.find(part) != std::string::npos;
    };
    CHECK(has(error_of(sl::ShipStartPolicy{std::vector<std::uint32_t>{1, 5}}, rs.rules),
              "5 is not a seat"));
    CHECK(has(error_of(sl::ShipStartPolicy{std::vector<std::uint32_t>{2}}, rs.rules),
              "fewer than two members"));
    CHECK(has(error_of(sl::ShipStartPolicy{std::vector<std::uint32_t>{3, 1}}, rs.rules),
              "not ascending and unique"));
    sim::Rules with_groups = rs.rules;
    with_groups.shared_visibility = {{0, 1}};
    CHECK(has(error_of(sl::ShipStartPolicy{std::vector<std::uint32_t>{1, 2}}, with_groups),
              "both in the rules and in the ship-start policy"));
    CHECK(error_of(sl::ShipStartPolicy{}, with_groups).empty()); // rules alone are honoured
}

TEST_CASE("scenario player DISCO on the shipped scenes: restored sets, sharing off by default") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // Scenes/0201SingleAddOn/single39.sze: a player record with DISCO 0e000000 (players 1, 2, 3)
    // right after BONUS (file offset 1814; docs/sim/shared-visibility.md 9).
    const fs::path file = *dir / "Scenes" / "0201SingleAddOn" / "single39.sze";
    if (!fs::exists(file)) {
        MESSAGE("skipping: " << file.string() << " not present");
        return;
    }
    const std::vector<std::uint8_t> bytes = read_all(file);
    const auto types = original_types(*dir);
    auto l = sim::load_scenario(bytes, *types, sim::SimConfig{});
    REQUIRE_MESSAGE(l.ok(), (l.ok() ? std::string() : l.error()));
    bool found = false;
    for (const sim::ScenarioSpec::PlayerSpec& p : l.value().spec.players) {
        if (std::binary_search(p.discovery.begin(), p.discovery.end(), sim::PlayerId{1}) &&
            std::binary_search(p.discovery.begin(), p.discovery.end(), sim::PlayerId{2}) &&
            std::binary_search(p.discovery.begin(), p.discovery.end(), sim::PlayerId{3})) {
            found = true;
        }
    }
    CHECK(found);
    auto w = sim::World::create(l.value().spec, types, sim::SimConfig{});
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    CHECK(w.value().rules().shared_visibility.empty()); // an ordinary scenario: sharing off
    CHECK(w.value().check_invariants().empty());
}

TEST_CASE("ship start canary: two participants found, a conflict is refused, replay reproduces") {
    // Headless end-to-end canary on the unchanged candidate (MAP-FOUND-3 E). Sailing is not
    // implemented: the test RELOCATES the ships of seats 0, 1 and 2 beside sites it validated
    // through the engine's own command BEFORE the recording starts (fixture setup, not a sailed
    // route), so the log's initial save already holds the relocated ships.
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    RealStart rs = real_start(*dir);
    const sim::BuildingType& kontor = *rs.types->find(sim::TypeId{1302});
    auto loaded = sl::load_ship_start(rs.manifest, *dir, rs.types, {}, rs.rules);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    sim::World w = std::move(loaded).value();
    // Five ships with crews (scout, donkey) and no city anywhere.
    CHECK(sl::expedition_baseline_problem(w).empty());
    for (const auto& island : w.islands()) {
        if (island) {
            for (const auto& c : island->cities) {
                CHECK_FALSE(c.has_value());
            }
        }
    }
    const auto ship_pos = [](const sim::World& world, std::uint64_t id) {
        return sim::find_figure(world.figures(), sim::FigureId{id})->pos;
    };
    const auto set_pos = [](sim::World& world, std::uint64_t id, sim::FixedPos pos) {
        relocate(*sim::find_figure(sim::WorldTestAccess::figures(world), sim::FigureId{id}), pos);
    };
    // Setup on a trial copy: site 1 for seat 0, then the next legal site (on the world after the
    // first founding) for seat 2.
    sim::World trial = w;
    const std::optional<Site> first = find_legal_site(trial, kontor, trial.config().neutral_owner);
    REQUIRE(first.has_value());
    const auto c0 = place_for_founding(trial, *first, 0);
    REQUIRE(c0.has_value());
    set_pos(w, 1, ship_pos(trial, 1));
    set_pos(w, 4, ship_pos(trial, 1)); // seat 1's ship sits beside the same site (the conflict)
    REQUIRE(trial.apply(*c0).ok());
    const std::optional<Site> second = find_legal_site(trial, kontor, trial.config().neutral_owner);
    REQUIRE(second.has_value());
    const auto c2 = place_for_founding(trial, *second, 2);
    REQUIRE(c2.has_value());
    set_pos(w, 7, ship_pos(trial, 7));
    REQUIRE_MESSAGE(w.check_invariants().empty(), w.check_invariants());

    auto started = sim::Recorder::start(w);
    REQUIRE(started.ok());
    sim::Recorder rec = std::move(started).value();
    REQUIRE(rec.submit(*c0).ok()); // the recorder applies the command to `w` and logs it

    // A conflicting command: seat 1 at the very same site is refused (owned tiles), uncharged,
    // and leaks no discovery (bytes and PRNG unchanged).
    {
        sim::FoundCmd same = std::get<sim::FoundCmd>(c0->body);
        same.ship = sim::FigureId{4};
        const std::vector<std::uint8_t> bytes = w.save();
        const std::uint32_t cursor = w.rng().cursor();
        const sim::ApplyResult refused = w.apply(sim::Command{sim::PlayerId{1}, same});
        CHECK(refused.status == sim::Reject::SiteOwned);
        CHECK(w.save() == bytes);
        CHECK(w.rng().cursor() == cursor);
    }
    const sim::World after_first = w;

    REQUIRE(rec.submit(*c2).ok());
    REQUIRE_MESSAGE(w.check_invariants().empty(), w.check_invariants());
    std::size_t cities = 0;
    for (const auto& island : w.islands()) {
        if (!island) {
            continue;
        }
        for (const auto& c : island->cities) {
            if (c.has_value()) {
                ++cities;
                CHECK((c->owner == sim::PlayerId{0} || c->owner == sim::PlayerId{2}));
                CHECK(c->player_mask == pid_set({0, 1, 2, 3, 4}));
            }
        }
    }
    CHECK(cities == 2);
    CHECK(w.players().find(sim::PlayerId{0})->money == 29650);
    CHECK(w.players().find(sim::PlayerId{2})->money == 29650);
    for (const std::uint32_t p : {1U, 3U, 4U}) {
        CHECK(w.players().find(sim::PlayerId{p})->money == 30000); // nobody else paid
    }
    for (std::uint32_t p = 0; p < 5; ++p) {
        CHECK(w.players().find(sim::PlayerId{p})->discovery == pid_set({0, 2}));
        CHECK(w.players().find(sim::PlayerId{p})->relations.empty());
        // The state after seat 0 alone did not yet know seat 2 (nothing pre-revealed).
        CHECK(after_first.players().find(sim::PlayerId{p})->discovery == pid_set({0}));
    }
    // Replay: the two recorded foundings plus a checkpoint reproduce in a fresh world built from
    // the log's own initial save.
    rec.advance(500);
    rec.checkpoint();
    auto read = sim::read_log(sim::write_log(rec.log()).value());
    REQUIRE(read.ok());
    auto fresh = sim::World::load(read.value().initial_save, rs.types, {});
    REQUIRE_MESSAGE(fresh.ok(), (fresh.ok() ? std::string() : fresh.error()));
    auto outcome = sim::replay(fresh.value(), read.value());
    REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
    CHECK(outcome.value().reproduced());
    CHECK(fresh.value().state_hash() == w.state_hash());
}
