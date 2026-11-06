#include "island_helpers.hpp"
#include "original_component.hpp"
// The founding command on the original files (task MAP-2 package 8d). The real registry
// (Anlagen.dat, FigurenHeader.dat), the real islands of the unchanged candidate lan5-ship-a through
// the ship-start loader, and the real STAEDTE name list. The legal site is FOUND by the engine's
// own site predicates, then the command is applied through World::apply; the candidate files are
// only read (compared before and after). Skips with a message without ANNO1503_GAME_DIR; the
// canonical gate sets it and treats a skip as a skip.
#include <algorithm>
#include <array>
#include <chrono>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/ship_manifest.hpp>
#include <anno/sim/build_availability.hpp>
#include <anno/sim/city_booking.hpp>
#include <anno/sim/command.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/harvest.hpp>
#include <anno/sim/placement.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/route.hpp>
#include <anno/sim/route_world.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/scenario_formats.hpp>
#include <anno/sim/ship_cargo.hpp>
#include <anno/sim/ship_step.hpp>
#include <anno/sim/site_checks.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/terrain_view.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/world.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/ship_start.hpp>
#include <anno/test/game_dir.hpp>

#include "cargo_transfer_oracle.hpp"
#include "command_helpers.hpp"
#include "evidence_out.hpp"
#include "island_helpers.hpp"
#include "world_access.hpp"

namespace {

/// Exports a replay log to the operator-named path (ANNO_SAIL_LOG*). Returns "" when no path was
/// requested or the file was published; otherwise the failing path and operation (MAP-BUILD-1 A).
[[nodiscard]] std::string export_log_if_requested(const char* path,
                                                  const std::vector<std::uint8_t>& bytes) {
    if (path == nullptr || *path == '\0') {
        return {};
    }
    return anno::sim::test::write_evidence_file(
        path, std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}

} // namespace

namespace {} // namespace

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
    // Type 391 has 200 hp and no holds at all: it navigates, its cargo term is 0 (12.8).
    REQUIRE(types->find_figure_type(391)->nav.has_value());
    CHECK(types->find_figure_type(391)->nav->hold_count_a == 0U);
    CHECK(types->find_figure_type(391)->nav->hold_count_b == 0U);
    CHECK(types->find_figure_type(391)->nav->max_hp == 200U);
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
    for (const std::pair<int, int>& d : {std::pair{6, 0}, std::pair{-6, 0}, std::pair{0, 6},
                                         std::pair{0, -6}, std::pair{7, 7}, std::pair{-7, -7}}) {
        sim::World trial = w;
        sim::Figure* s = sim::find_figure(sim::WorldTestAccess::figures(trial), sim::FigureId{1});
        s->pos.x = (found->click_x + d.first) * 256 + 128;
        s->pos.y = (found->click_y + d.second) * 256 + 128;
        if (trial.apply(command).ok()) {
            sim::WorldTestAccess::relocate(w, sim::FigureId{1}, s->pos);
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
        owned += after.tiles.owner(i) == 0 ? 1U : 0U;
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
    sim::WorldTestAccess::relocate(w2, sim::FigureId{1},
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
    for (const std::pair<int, int>& d : {std::pair{6, 0}, std::pair{-6, 0}, std::pair{0, 6},
                                         std::pair{0, -6}, std::pair{7, 7}, std::pair{-7, -7}}) {
        sim::World trial = w;
        sim::Figure* s = sim::find_figure(sim::WorldTestAccess::figures(trial), cmd.ship);
        s->pos.x = (site.click_x + d.first) * 256 + 128;
        s->pos.y = (site.click_y + d.second) * 256 + 128;
        if (trial.apply(command).ok()) {
            sim::WorldTestAccess::relocate(w, cmd.ship, s->pos);
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
        const auto* const now = w.players().find(sim::PlayerId{p});
        const auto* const before = before_founding.players().find(sim::PlayerId{p});
        REQUIRE(now != nullptr);
        REQUIRE(before != nullptr);
        CHECK(now->discovery == pid_set({0}));
        CHECK(now->relations.empty()); // still no alliance
        CHECK(now->money == (p == 0 ? 29650 : before->money));
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
        sim::WorldTestAccess::relocate(w3, sim::FigureId{1},
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
        sim::WorldTestAccess::relocate(w4, sim::FigureId{1},
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

TEST_CASE("scenario player DISCO original-data component: restored sets, sharing off by default") {
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
    auto l =
        sim::load_scenario(bytes, *types, sim::SimConfig{}, original_component::actor_free_units());
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
    auto component = original_component::import_only(l.value().spec, types);
    REQUIRE_MESSAGE(component, (component ? "" : component.error()));
    MESSAGE("original-data DISCO component: " << component.value().diagnostic);
    auto w = sim::World::create(component.value().spec, types, sim::SimConfig{});
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
        sim::WorldTestAccess::relocate(world, sim::FigureId{id}, pos);
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

// ---- MAP-NAV-2 package F: the sailed founding canary -------------------------------------------
// Two participants sail from their unchanged ship starts to water tiles beside legal sites and
// found their cities there, through MoveCmd, the real stepper and route engine on the real
// terrain, and FoundCmd. Nothing is relocated: the ships are never written by the test. The sites
// and the water tiles are searched on COPIES of the world (a trial may move a ship; the real run
// never does) and each goal is verified reachable by the route engine itself before the run.

namespace {

struct SailLeg {
    std::uint32_t player = 0;
    std::uint64_t ship = 0;
    Site site;
    int goal_x = 0, goal_y = 0; // the water tile the ship sails to
};

sim::Command found_command(const SailLeg& leg) {
    sim::FoundCmd cmd;
    cmd.ship = sim::FigureId{leg.ship};
    cmd.placement.type = sim::TypeId{1302};
    cmd.placement.rotation = 0;
    cmd.placement.x = leg.site.click_x - 1;
    cmd.placement.y = leg.site.click_y - 1;
    cmd.click_x = leg.site.click_x;
    cmd.click_y = leg.site.click_y;
    return sim::Command{sim::PlayerId{leg.player}, cmd};
}

sim::Command sail_command(const SailLeg& leg) {
    return sim::Command{
        sim::PlayerId{leg.player},
        sim::MoveCmd{sim::FigureId{leg.ship}, leg.goal_x * 256 + 128, leg.goal_y * 256 + 128}};
}

/// The nearest water tiles to the click (ring by ring) for which a trial copy accepts the founding
/// with the ship standing there AND the route engine plans the ship's way there exactly. `trial`
/// is the world the founding would be applied to (the first leg's trial already holds city 0).
std::optional<std::pair<int, int>> pick_water_goal(const sim::World& real, const sim::World& trial,
                                                   SailLeg leg) {
    const sim::WorldTileSource src(real, sim::ship_route_terrain_options());
    const sim::CoastIndex index(src, {});
    const sim::Figure* ship = sim::find_figure(real.figures(), sim::FigureId{leg.ship});
    REQUIRE(ship != nullptr);
    struct Cand {
        int d, x, y;
    };
    std::vector<Cand> cands;
    for (int dy = -14; dy <= 14; ++dy) {
        for (int dx = -14; dx <= 14; ++dx) {
            const int x = leg.site.click_x + dx, y = leg.site.click_y + dy;
            if (x >= 0 && y >= 0 && src.tile_is_water(x, y)) {
                cands.push_back({std::max(std::abs(dx), std::abs(dy)), x, y});
            }
        }
    }
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
        return a.d != b.d ? a.d < b.d : (a.y != b.y ? a.y < b.y : a.x < b.x);
    });
    int tried = 0;
    for (const Cand& c : cands) {
        if (c.d < 4 || tried++ > 400) {
            continue; // keep a hull's distance from the shore, and bound the search
        }
        sim::World t = trial;
        sim::WorldTestAccess::relocate(
            t, sim::FigureId{leg.ship},
            sim::FixedPos{c.x * 256 + 128, c.y * 256 + 128}); // trial only
        if (!t.apply(found_command(leg)).ok()) {
            continue;
        }
        sim::RouteQuery q;
        q.start_x = ship->pos.x >> 8;
        q.start_z = ship->pos.y >> 8;
        q.goal_x_q8 = c.x * 256 + 128;
        q.goal_z_q8 = c.y * 256 + 128;
        const sim::RouteResult r =
            sim::plan_route(index, q, sim::RouteLimits{}, sim::RouteCostTable::original());
        if (r.status == sim::RouteStatus::Planned && r.end_x == c.x && r.end_z == c.y) {
            return std::pair{c.x, c.y};
        }
    }
    return std::nullopt;
}

struct Driver {
    sim::World* w = nullptr;
    sim::Recorder* rec = nullptr;
    sim::ApplyResult apply(const sim::Command& c) { return rec ? rec->submit(c) : w->apply(c); }
    void advance(std::uint32_t ms) {
        if (rec) {
            rec->advance(ms);
        } else {
            w->advance(ms);
        }
    }
    void checkpoint() {
        if (rec) {
            rec->checkpoint();
        }
    }
};

struct SailOutcome {
    std::uint64_t final_hash = 0;
    std::uint64_t arrival_advances = 0;
    /// The worst excess of a position change over what the elapsed ticks allow at full speed
    /// (0.036 tile per 5 ticks = 1.8432 Q24.8 units per tick, +1 for the floor); <= 0 means no
    /// jump. The registry visits a ship every n / k Advances, so a single Advance can carry many
    /// ticks.
    std::int64_t worst_excess_q8 = 0;
    std::int32_t worst_step_q8 = 0;
    std::vector<double> advance_us;
    bool all_arrived = false;
    std::uint32_t failures = 0;
};

/// Orders both legs, advances (10 ms = 5 ticks) until both ships reported ShipArrived, founds,
/// advances 500 ms more. `reload_at` > 0 saves and reloads the world after that many advances.
SailOutcome run_sail(sim::World& w, sim::Recorder* rec, const std::vector<SailLeg>& legs,
                     const std::shared_ptr<const sim::TypeRegistry>& types, std::uint64_t reload_at,
                     bool timing) {
    Driver d{&w, rec};
    SailOutcome out;
    for (const SailLeg& leg : legs) {
        REQUIRE(d.apply(sail_command(leg)).ok());
    }
    std::vector<sim::FixedPos> last;
    std::vector<std::uint16_t> last_visit;
    for (const SailLeg& leg : legs) {
        const sim::Figure* f = sim::find_figure(w.figures(), sim::FigureId{leg.ship});
        last.push_back(f->pos);
        last_visit.push_back(f->nav->last);
    }
    std::size_t arrived = 0;
    (void)w.drain_events();
    for (std::uint64_t adv = 1; adv <= 40000 && arrived < legs.size(); ++adv) {
        const auto t0 = std::chrono::steady_clock::now();
        d.advance(10);
        if (timing) {
            out.advance_us.push_back(
                std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0)
                    .count());
        }
        for (const sim::SimEvent& e : w.drain_events()) {
            if (e.kind == sim::SimEventKind::ShipArrived) {
                ++arrived;
            } else if (e.kind == sim::SimEventKind::ShipRouteFailed) {
                ++out.failures;
                arrived = legs.size(); // stop: the canary reports the failure
            }
        }
        for (std::size_t i = 0; i < legs.size(); ++i) {
            const sim::Figure* f = sim::find_figure(w.figures(), sim::FigureId{legs[i].ship});
            const std::int32_t step =
                std::max(std::abs(f->pos.x - last[i].x), std::abs(f->pos.y - last[i].y));
            const auto ticks = static_cast<std::uint16_t>(f->nav->last - last_visit[i]);
            out.worst_step_q8 = std::max(out.worst_step_q8, step);
            out.worst_excess_q8 = std::max<std::int64_t>(
                out.worst_excess_q8, step - (std::int64_t{ticks} * 18432 / 10000 + 1));
            last[i] = f->pos;
            last_visit[i] = f->nav->last;
        }
        if (adv % 2000 == 0) {
            d.checkpoint();
        }
        if (reload_at != 0 && adv == reload_at) {
            auto back = sim::World::load(w.save(), types);
            REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
            REQUIRE(back.value().state_hash() == w.state_hash());
            w = std::move(back).value();
        }
        out.arrival_advances = adv;
    }
    out.all_arrived = out.failures == 0 && arrived == legs.size();
    d.checkpoint();
    for (const SailLeg& leg : legs) {
        REQUIRE_MESSAGE(d.apply(found_command(leg)).ok(), "founding after the voyage was refused");
    }
    d.advance(500);
    d.checkpoint();
    out.final_hash = w.state_hash();
    return out;
}

} // namespace

TEST_CASE(
    "sailed founding canary: two participants sail and found, no relocation, replay reproduces") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    RealStart rs = real_start(*dir);
    const sim::BuildingType& kontor = *rs.types->find(sim::TypeId{1302});
    auto loaded = sl::load_ship_start(rs.manifest, *dir, rs.types, {}, rs.rules);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    const sim::World pristine = std::move(loaded).value();
    REQUIRE(sl::expedition_baseline_problem(pristine).empty());
    const std::uint64_t pristine_hash = pristine.state_hash();

    // ---- setup on copies: the sites and the water tiles beside them ---------------------------
    std::vector<SailLeg> legs(2);
    legs[0].player = 0;
    legs[0].ship = 1; // seat 0
    legs[1].player = 2;
    legs[1].ship = 7; // seat 2
    const auto first_site = find_legal_site(pristine, kontor, pristine.config().neutral_owner);
    REQUIRE(first_site.has_value());
    legs[0].site = *first_site;
    const auto g0 = pick_water_goal(pristine, pristine, legs[0]);
    REQUIRE_MESSAGE(g0.has_value(), "no reachable water tile beside the first site");
    legs[0].goal_x = g0->first;
    legs[0].goal_y = g0->second;
    sim::World trial = pristine;
    sim::WorldTestAccess::relocate(
        trial, sim::FigureId{1},
        sim::FixedPos{g0->first * 256 + 128, g0->second * 256 + 128}); // trial only
    REQUIRE(trial.apply(found_command(legs[0])).ok());
    const auto second_site = find_legal_site(trial, kontor, trial.config().neutral_owner);
    REQUIRE(second_site.has_value());
    legs[1].site = *second_site;
    const auto g1 = pick_water_goal(pristine, trial, legs[1]);
    REQUIRE_MESSAGE(g1.has_value(), "no reachable water tile beside the second site");
    legs[1].goal_x = g1->first;
    legs[1].goal_y = g1->second;
    for (const SailLeg& leg : legs) {
        const sim::Figure* s = sim::find_figure(pristine.figures(), sim::FigureId{leg.ship});
        const int dist =
            std::max(std::abs(s->pos.x / 256 - leg.goal_x), std::abs(s->pos.y / 256 - leg.goal_y));
        MESSAGE("seat " << leg.player << ": start (" << s->pos.x / 256 << ", " << s->pos.y / 256
                        << ") -> water (" << leg.goal_x << ", " << leg.goal_y << ") beside click ("
                        << leg.site.click_x << ", " << leg.site.click_y << "), " << dist
                        << " tiles");
        CHECK(dist >= 20); // a real voyage, not a hop
    }

    // ---- measurements: the route queries of both legs, and an idle baseline of the advance ----
    {
        const auto t0 = std::chrono::steady_clock::now();
        const sim::WorldTileSource src(pristine, sim::ship_route_terrain_options());
        const sim::CoastIndex index(src, {});
        const double build_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                .count();
        MESSAGE("coast index: " << index.allocated_sectors() << " sectors, built in " << build_ms
                                << " ms");
        for (const SailLeg& leg : legs) {
            const sim::Figure* s = sim::find_figure(pristine.figures(), sim::FigureId{leg.ship});
            sim::RouteQuery q;
            q.start_x = s->pos.x >> 8;
            q.start_z = s->pos.y >> 8;
            q.goal_x_q8 = leg.goal_x * 256 + 128;
            q.goal_z_q8 = leg.goal_y * 256 + 128;
            const auto q0 = std::chrono::steady_clock::now();
            const sim::RouteResult r =
                sim::plan_route(index, q, sim::RouteLimits{}, sim::RouteCostTable::original());
            const double ms =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - q0)
                    .count();
            MESSAGE("route seat " << leg.player << ": status " << static_cast<int>(r.status) << ", "
                                  << r.runs.size() << " runs, coarse pops " << r.work.coarse_pops
                                  << ", fine pops " << r.work.fine_pops << ", learned "
                                  << r.work.learned << ", bytes_high " << r.work.bytes_high << ", "
                                  << ms << " ms");
            CHECK(r.status == sim::RouteStatus::Planned);
        }
        sim::World idle = pristine;
        std::vector<double> idle_us;
        for (int i = 0; i < 3000; ++i) {
            const auto a0 = std::chrono::steady_clock::now();
            idle.advance(10);
            idle_us.push_back(
                std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - a0)
                    .count());
        }
        std::sort(idle_us.begin(), idle_us.end());
        MESSAGE("idle baseline (no orders), advance wall time us: p50 "
                << idle_us[idle_us.size() / 2] << " p95 " << idle_us[idle_us.size() * 95 / 100]
                << " p99 " << idle_us[idle_us.size() * 99 / 100] << " max " << idle_us.back());
    }

    // ---- the recorded run: sail, found, replay ------------------------------------------------
    sim::World w = pristine;
    auto started = sim::Recorder::start(w);
    REQUIRE(started.ok());
    sim::Recorder rec = std::move(started).value();
    SailOutcome main_run = run_sail(w, &rec, legs, rs.types, 0, true);
    REQUIRE_MESSAGE(main_run.failures == 0, "a route failed");
    REQUIRE(main_run.all_arrived);
    // No teleport: every position change is within what the ticks since the ship's last visit
    // allow.
    CHECK(main_run.worst_excess_q8 <= 0);
    MESSAGE("largest single position change: " << main_run.worst_step_q8 << " Q24.8 units");
    REQUIRE_MESSAGE(w.check_invariants().empty(), w.check_invariants());
    // The founding happened where the ships stand now (the unchanged ship objects).
    std::size_t cities = 0;
    for (const auto& island : w.islands()) {
        for (const auto& c : island ? island->cities : decltype(island->cities){}) {
            cities += c.has_value() ? 1U : 0U;
        }
    }
    CHECK(cities == 2);
    CHECK(w.players().find(sim::PlayerId{0})->money == 29650);
    CHECK(w.players().find(sim::PlayerId{2})->money == 29650);
    for (const std::uint32_t p : {1U, 3U, 4U}) {
        CHECK(w.players().find(sim::PlayerId{p})->money == 30000);
    }
    for (std::uint32_t p = 0; p < 5; ++p) {
        CHECK(w.players().find(sim::PlayerId{p})->discovery == pid_set({0, 2}));
        CHECK(w.players().find(sim::PlayerId{p})->relations.empty());
    }
    for (const SailLeg& leg : legs) { // the crew rides with its ship
        const sim::Figure* s = sim::find_figure(w.figures(), sim::FigureId{leg.ship});
        CHECK(sim::find_figure(w.figures(), sim::FigureId{leg.ship + 1})->pos.x == s->pos.x);
        CHECK(sim::find_figure(w.figures(), sim::FigureId{leg.ship + 2})->pos.y == s->pos.y);
        CHECK(s->pos.x / 256 == leg.goal_x); // it stands on the goal tile
        CHECK(s->pos.y / 256 == leg.goal_y);
    }
    // Ships of seats 1, 3, 4 never moved.
    for (const std::uint64_t id : {4ULL, 10ULL, 13ULL}) {
        const sim::FixedPos now = sim::find_figure(w.figures(), sim::FigureId{id})->pos;
        const sim::FixedPos then = sim::find_figure(pristine.figures(), sim::FigureId{id})->pos;
        CHECK((now.x == then.x && now.y == then.y));
    }

    // Measurements of this run (reported, not asserted against a guess).
    std::vector<double> us = main_run.advance_us;
    std::sort(us.begin(), us.end());
    const auto pct = [&](double p) {
        return us[std::min(us.size() - 1,
                           static_cast<std::size_t>(p * static_cast<double>(us.size())))];
    };
#if defined(__unix__) || defined(__APPLE__)
    {
        struct rusage ru{};
        getrusage(RUSAGE_SELF, &ru);
        MESSAGE("process peak resident set so far: " << ru.ru_maxrss
                                                     << " (bytes on macOS, KiB on Linux)");
    }
#endif
    MESSAGE("sail canary: " << main_run.arrival_advances << " advances ("
                            << main_run.arrival_advances * 5
                            << " ticks); advance wall time us: p50 " << pct(0.50) << " p95 "
                            << pct(0.95) << " p99 " << pct(0.99) << " max " << us.back());

    // ---- replay in a fresh world from the log, through bytes, and a tampered log --------------
    auto bytes = sim::write_log(rec.log());
    REQUIRE(bytes.ok());
    {
        const std::string export_error =
            export_log_if_requested(std::getenv("ANNO_SAIL_LOG"), bytes.value());
        REQUIRE_MESSAGE(export_error.empty(), export_error);
    }
    auto read = sim::read_log(bytes.value());
    REQUIRE(read.ok());
    auto fresh = sim::World::load(read.value().initial_save, rs.types, {});
    REQUIRE_MESSAGE(fresh.ok(), (fresh.ok() ? std::string() : fresh.error()));
    CHECK(fresh.value().state_hash() == pristine_hash);
    auto outcome = sim::replay(fresh.value(), read.value());
    REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
    CHECK(outcome.value().reproduced());
    CHECK(fresh.value().state_hash() == w.state_hash());
    CHECK(main_run.final_hash == w.state_hash());
    {
        // A changed target is discriminating: edit the first order in the log, replay no longer
        // reproduces the recorded checkpoints.
        sim::ReplayLog tampered = read.value();
        bool edited = false;
        for (sim::ReplayEntry& e : tampered.entries) {
            for (sim::Command& c : e.commands) {
                if (auto* mv = std::get_if<sim::MoveCmd>(&c.body); mv != nullptr && !edited) {
                    mv->target_x_q8 += 3 * 256;
                    edited = true;
                }
            }
        }
        REQUIRE(edited);
        if (const char* bad_path = std::getenv("ANNO_SAIL_LOG_TAMPERED");
            bad_path != nullptr && *bad_path) {
            const auto bad_bytes = sim::write_log(tampered);
            REQUIRE(bad_bytes.ok());
            const std::string export_error = export_log_if_requested(bad_path, bad_bytes.value());
            REQUIRE_MESSAGE(export_error.empty(), export_error);
        }
        auto fresh2 = sim::World::load(tampered.initial_save, rs.types, {});
        REQUIRE(fresh2.ok());
        auto bad = sim::replay(fresh2.value(), tampered);
        CHECK((!bad.ok() || !bad.value().reproduced()));
    }

    // ---- mid-voyage save and load, same voyage -------------------------------------------------
    {
        sim::World w2 = pristine;
        SailOutcome reloaded =
            run_sail(w2, nullptr, legs, rs.types, main_run.arrival_advances / 2, false);
        CHECK(reloaded.all_arrived);
        CHECK(reloaded.arrival_advances == main_run.arrival_advances);
        CHECK(reloaded.final_hash == main_run.final_hash);
    }
    // ---- a different target gives a different voyage ------------------------------------------
    {
        sim::World w3 = pristine;
        std::vector<SailLeg> other = legs;
        other[0].goal_x += 1; // a neighbouring tile east (or west of the original)
        Driver d{&w3, nullptr};
        REQUIRE(d.apply(sail_command(legs[0])).ok());
        sim::World w4 = pristine;
        Driver d4{&w4, nullptr};
        REQUIRE(d4.apply(sail_command(other[0])).ok());
        for (int i = 0; i < 600; ++i) {
            w3.advance(10);
            w4.advance(10);
        }
        CHECK(w3.state_hash() != w4.state_hash());
    }
}

TEST_CASE("sailed founding canary: a real route failure leaves the ship idle with its event") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    RealStart rs = real_start(*dir);
    auto loaded = sl::load_ship_start(rs.manifest, *dir, rs.types, {}, rs.rules);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    sim::World w = std::move(loaded).value();
    // A land tile in the middle of an island: no water within 8 tiles (the goal radius is 6), so
    // the order is accepted (the target is inside the world) and the plan fails.
    const sim::WorldTileSource src(w, sim::ship_route_terrain_options());
    std::optional<std::pair<int, int>> inland;
    for (const auto& island : w.islands()) {
        if (!island || inland.has_value()) {
            continue;
        }
        const int x0 = island->tiles.x0(), y0 = island->tiles.y0();
        const int ww = static_cast<int>(island->tiles.width()),
                  hh = static_cast<int>(island->tiles.height());
        for (int y = y0 + 9; y < y0 + hh - 9 && !inland; y += 2) {
            for (int x = x0 + 9; x < x0 + ww - 9 && !inland; x += 2) {
                bool dry = true;
                for (int dy = -8; dy <= 8 && dry; ++dy) {
                    for (int dx = -8; dx <= 8 && dry; ++dx) {
                        dry = !src.tile_is_water(x + dx, y + dy);
                    }
                }
                if (dry) {
                    inland = std::pair{x, y};
                }
            }
        }
    }
    REQUIRE_MESSAGE(inland.has_value(),
                    "no island of the candidate has an interior 17 x 17 land block");
    const sim::Figure* ship = sim::find_figure(w.figures(), sim::FigureId{1});
    const sim::FixedPos start = ship->pos;
    const std::uint64_t order_before = w.ids().next_order_id;
    REQUIRE(w.apply(sim::Command{sim::PlayerId{0},
                                 sim::MoveCmd{sim::FigureId{1}, inland->first * 256 + 128,
                                              inland->second * 256 + 128}})
                .ok());
    (void)w.drain_events();
    bool failed = false;
    for (int i = 0; i < 400 && !failed; ++i) {
        w.advance(10);
        for (const sim::SimEvent& e : w.drain_events()) {
            if (e.kind == sim::SimEventKind::ShipRouteFailed) {
                failed = true;
                CHECK(e.order_id == order_before);
                CHECK(e.code == static_cast<std::uint32_t>(sim::NavFailReason::Unreachable));
            }
            CHECK(e.kind != sim::SimEventKind::ShipArrived);
        }
    }
    REQUIRE(failed);
    const sim::Figure* after = sim::find_figure(w.figures(), sim::FigureId{1});
    CHECK_FALSE(after->nav->order.has_value());
    CHECK_FALSE(after->nav->ticket.has_value());
    CHECK(after->pos.x / 256 == start.x / 256); // it did not leave its tile
    CHECK(after->pos.y / 256 == start.y / 256);
    CHECK(w.check_invariants().empty());
}

TEST_CASE("sail log export: a requested path that cannot be written fails with its path") {
    namespace fs = std::filesystem;
    const fs::path dir = anno::sim::test::make_private_dir("anno_sail_export_test");
    REQUIRE_FALSE(dir.empty());
    const std::vector<std::uint8_t> payload = {1, 2, 3, 250, 0, 7};
    // Valid control: published, byte-identical, no partial file left behind.
    CHECK(export_log_if_requested((dir / "ok.bin").string().c_str(), payload).empty());
    {
        std::ifstream f(dir / "ok.bin", std::ios::binary);
        const std::vector<std::uint8_t> back((std::istreambuf_iterator<char>(f)),
                                             std::istreambuf_iterator<char>());
        CHECK(back == payload);
    }
    CHECK_FALSE(fs::exists(dir / "ok.bin.partial"));
    // No request means nothing to do and nothing to fail.
    CHECK(export_log_if_requested(nullptr, payload).empty());
    CHECK(export_log_if_requested("", payload).empty());
    // Negative controls: the destination is a directory; the parent does not exist.
    fs::create_directories(dir / "is_dir");
    const std::string dir_err = export_log_if_requested((dir / "is_dir").string().c_str(), payload);
    CHECK(dir_err.find("is_dir") != std::string::npos);
    CHECK(dir_err.find("directory") != std::string::npos);
    const std::string open_err =
        export_log_if_requested((dir / "missing" / "x.bin").string().c_str(), payload);
    CHECK(open_err.find("open failed") != std::string::npos);
    // A failed publication removes the earlier run's file instead of leaving it looking current.
    CHECK(export_log_if_requested((dir / "stale.bin").string().c_str(), payload).empty());
    fs::create_directories(dir / "stale.bin.partial"); // the temporary name is taken: open fails
    CHECK_FALSE(export_log_if_requested((dir / "stale.bin").string().c_str(), payload).empty());
    CHECK_FALSE(fs::exists(dir / "stale.bin"));
    fs::remove_all(dir);
}

// ---- MAP-BUILD-1 package D4/D5: ordinary buildings on the real terrain --------------------------
// D4.1 asks first whether the city a ship founds can afford a supported ordinary building. It
// cannot (the founding leaves an empty warehouse; the ship's cargo is not city stock and nothing
// transfers it), so the combined sail -> found -> build loop is BLOCKED ON UNLOADING: the sailed
// canary above stays unchanged and the build is shown on a SEPARATE, explicitly stocked fixture:
// the real terrain of a shipped island and the original registry, a city whose stock is declared
// in the ScenarioSpec before the world exists. No post-founding grant, no zero-cost type change,
// no transfer. Everything after creation goes through public commands (World::apply).

namespace {

constexpr std::uint32_t kPionier = 501, kHandwerk = 1001;
/// MAP-ROAD-1: the dirt road (STRASSE, 0x4186, 5 gold, walk pair (0, 3)); Anlagen.dat id 2520.
constexpr std::uint32_t kRoadDirt = 2520;

struct Origin {
    std::int32_t x = 0, y = 0;
    std::uint8_t height = 0;
};

std::int64_t gold_of(const sim::World& w, std::uint32_t p) {
    const auto* const player = w.players().find(sim::PlayerId{p});
    REQUIRE(player != nullptr);
    return player->money;
}

/// Every origin of the island where the placement rule accepts `type` (rotation 0), row-major.
std::vector<Origin> legal_origins(const sim::Island& is, const sim::TypeRegistry& types,
                                  const sim::BuildingType& type,
                                  const sim::PlacementParams& params = {}) {
    std::vector<Origin> out;
    for (std::int32_t y = is.tiles.y0();
         y < is.tiles.y0() + static_cast<std::int32_t>(is.tiles.height()); ++y) {
        for (std::int32_t x = is.tiles.x0();
             x < is.tiles.x0() + static_cast<std::int32_t>(is.tiles.width()); ++x) {
            sim::PlacementRecord rec;
            rec.type = type.id;
            rec.x = x;
            rec.y = y;
            const sim::PlacementResult r = sim::placement_rule(is, types, type, rec, params);
            if (r.reject == sim::Reject::None) {
                out.push_back({x, y, r.height});
            }
        }
    }
    return out;
}

/// The island of a fixture: the REAL terrain (the same shared Terrain object), geometry, climate
/// and ore objects of `real`; no scenery (so every site is decided by the terrain alone). Tiles of
/// city 0 are the rectangle [rx0, rx1] x [ry0, ry1] (world coordinates), the rest is neutral.
sim::ScenarioSpec::IslandSpec fixture_island(const sim::Island& real, std::int32_t rx0,
                                             std::int32_t ry0, std::int32_t rx1, std::int32_t ry1) {
    sim::ScenarioSpec::IslandSpec is = anno::sim::synthetic::plain_island(
        0, real.tiles.x0(), real.tiles.y0(), real.tiles.width(), real.tiles.height());
    is.climate = real.climate;
    is.terrain = real.terrain;
    is.ore_objects = real.ore_objects;
    constexpr std::uint32_t kNeutral = 15;
    for (std::int32_t y = real.tiles.y0();
         y < real.tiles.y0() + static_cast<std::int32_t>(real.tiles.height()); ++y) {
        const auto left = static_cast<std::uint32_t>(
            std::clamp(rx0 - real.tiles.x0(), 0, static_cast<std::int32_t>(real.tiles.width())));
        if (y < ry0 || y > ry1) {
            is.owner_runs.emplace_back(kNeutral, real.tiles.width());
            continue;
        }
        const auto mine = static_cast<std::uint32_t>(rx1 - rx0 + 1);
        if (left > 0) {
            is.owner_runs.emplace_back(kNeutral, left);
        }
        is.owner_runs.emplace_back(0U, mine);
        if (left + mine < real.tiles.width()) {
            is.owner_runs.emplace_back(kNeutral, real.tiles.width() - left - mine);
        }
    }
    is.cities.push_back(anno::sim::synthetic::plain_city(0, 0));
    return is;
}

bool apart(const Origin& a, std::uint32_t aw, std::uint32_t ah, const Origin& b, std::uint32_t bw,
           std::uint32_t bh) { // disjoint with a one tile margin
    return a.x + static_cast<std::int32_t>(aw) + 1 <= b.x ||
           b.x + static_cast<std::int32_t>(bw) + 1 <= a.x ||
           a.y + static_cast<std::int32_t>(ah) + 1 <= b.y ||
           b.y + static_cast<std::int32_t>(bh) + 1 <= a.y;
}

struct BuildFixture {
    std::size_t island = 0; // slot in the real start
    Origin a, b, sea;       // PIONIER site, HANDWERK site, a PIONIER site the terrain refuses
    sim::ScenarioSpec spec;
    sim::Rules rules;
};

/// Searches the real islands (in table order) for a PIONIER site A, a HANDWERK site B beside it
/// and a terrain-refused PIONIER origin near both, all decided by the engine's own placement rule
/// on a probe world that owns the whole island. Then builds the real fixture: the city owns only
/// the bounding rectangle of the three footprints plus a margin.
std::optional<BuildFixture>
find_build_fixture(const sim::World& real_start,
                   const std::shared_ptr<const sim::TypeRegistry>& types, std::int64_t edge_w,
                   std::int64_t edge_h, std::int64_t gold, std::uint32_t wood_t,
                   std::uint32_t tools_t) {
    namespace syn = anno::sim::synthetic;
    const sim::BuildingType& pionier = *types->find(sim::TypeId{kPionier});
    const sim::BuildingType& handwerk = *types->find(sim::TypeId{kHandwerk});
    const auto [pw, ph] = sim::footprint(pionier, 0);
    const auto [hw, hh] = sim::footprint(handwerk, 0);
    const auto wood = *types->goods_by_name("HOLZ");
    const auto tools = *types->goods_by_name("WERKZEUG");
    for (std::size_t i = 0; i < real_start.islands().size(); ++i) {
        const auto& slot = real_start.islands()[i];
        if (!slot.has_value() || slot->terrain == nullptr || slot->locked()) {
            continue;
        }
        BuildFixture fx;
        fx.island = i;
        fx.rules.shared_visibility.push_back({0, 1}); // the two players share information
        sim::ScenarioSpec probe = syn::empty_scenario(7);
        probe.settings.world_width = static_cast<std::uint32_t>(edge_w);
        probe.settings.world_height = static_cast<std::uint32_t>(edge_h);
        for (std::uint32_t p = 0; p < 2; ++p) {
            sim::ScenarioSpec::PlayerSpec ps;
            ps.id = sim::PlayerId{p};
            probe.players.push_back(ps);
        }
        probe.islands.push_back(
            fixture_island(*slot, slot->tiles.x0(), slot->tiles.y0(),
                           slot->tiles.x0() + static_cast<std::int32_t>(slot->tiles.width()) - 1,
                           slot->tiles.y0() + static_cast<std::int32_t>(slot->tiles.height()) - 1));
        auto pw_world = sim::World::create(probe, types, {}, fx.rules);
        if (!pw_world.ok()) {
            continue;
        }
        const sim::Island& pi = *pw_world.value().islands()[0];
        const std::vector<Origin> as = legal_origins(pi, *types, pionier);
        const std::vector<Origin> bs = legal_origins(pi, *types, handwerk);
        std::size_t tried = 0;
        for (const Origin& a : as) {
            if (++tried > 3000) {
                break;
            }
            const Origin* b = nullptr;
            for (const Origin& c : bs) {
                if (apart(a, pw, ph, c, hw, hh) && std::abs(c.x - a.x) <= 12 &&
                    std::abs(c.y - a.y) <= 12) {
                    b = &c;
                    break;
                }
            }
            if (b == nullptr) {
                continue;
            }
            // The nearest origin where the rule says TerrainRefused (the sea), apart from A and B.
            std::optional<Origin> sea;
            int best = 1 << 30;
            for (std::int32_t dy = -16; dy <= 16; ++dy) {
                for (std::int32_t dx = -16; dx <= 16; ++dx) {
                    Origin s{a.x + dx, a.y + dy, 0};
                    sim::PlacementRecord rec;
                    rec.type = pionier.id;
                    rec.x = s.x;
                    rec.y = s.y;
                    if (!apart(a, pw, ph, s, pw, ph) || !apart(*b, hw, hh, s, pw, ph) ||
                        sim::placement_rule(pi, *types, pionier, rec).reject !=
                            sim::Reject::TerrainRefused) {
                        continue;
                    }
                    const int d = std::max(std::abs(dx), std::abs(dy));
                    if (d < best) {
                        best = d;
                        sea = s;
                    }
                }
            }
            if (!sea.has_value()) {
                continue;
            }
            fx.a = a;
            fx.b = *b;
            fx.sea = *sea;
            const std::int32_t rx0 = std::min({a.x, b->x, sea->x}) - 1;
            const std::int32_t ry0 = std::min({a.y, b->y, sea->y}) - 1;
            const std::int32_t rx1 =
                std::max({a.x + static_cast<std::int32_t>(pw), b->x + static_cast<std::int32_t>(hw),
                          sea->x + static_cast<std::int32_t>(pw)});
            const std::int32_t ry1 =
                std::max({a.y + static_cast<std::int32_t>(ph), b->y + static_cast<std::int32_t>(hh),
                          sea->y + static_cast<std::int32_t>(ph)});
            if (rx0 < slot->tiles.x0() || ry0 < slot->tiles.y0() ||
                rx1 >= slot->tiles.x0() + static_cast<std::int32_t>(slot->tiles.width()) ||
                ry1 >= slot->tiles.y0() + static_cast<std::int32_t>(slot->tiles.height())) {
                continue;
            }
            // The scenario: two players with declared gold, the declared warehouse stock of the
            // city (8.8 tons), the shared audience of city 0.
            fx.spec = syn::empty_scenario(7);
            fx.spec.settings.world_width = static_cast<std::uint32_t>(edge_w);
            fx.spec.settings.world_height = static_cast<std::uint32_t>(edge_h);
            for (std::uint32_t p = 0; p < 2; ++p) {
                sim::ScenarioSpec::PlayerSpec ps;
                ps.id = sim::PlayerId{p};
                ps.money = gold;
                fx.spec.players.push_back(ps);
            }
            sim::ScenarioSpec::IslandSpec is = fixture_island(*slot, rx0, ry0, rx1, ry1);
            is.players_present = {sim::PlayerId{0}, sim::PlayerId{1}};
            std::vector<std::uint32_t> stock(sim::kOriginalWarehouseGoods, 0);
            stock.at(wood) = wood_t * sim::kTon;
            stock.at(tools) = tools_t * sim::kTon;
            is.cities[0].stock = stock;
            is.cities[0].player_mask = {sim::PlayerId{0}, sim::PlayerId{1}};
            fx.spec.islands.push_back(std::move(is));
            return fx;
        }
    }
    return std::nullopt;
}

} // namespace

TEST_CASE("build after founding: the founded city holds nothing, so no ordinary building is "
          "affordable (sail -> found -> build is BLOCKED on unloading)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    RealStart rs = real_start(*dir);
    auto loaded = sl::load_ship_start(rs.manifest, *dir, rs.types, {}, rs.rules);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    sim::World w = std::move(loaded).value();
    const sim::BuildingType& kontor = *rs.types->find(sim::TypeId{1302});
    const auto s = find_legal_site(w, kontor, w.config().neutral_owner);
    REQUIRE(s.has_value());
    // The founding command of the sailed canary (seat 0's ship, unchanged data), applied on a copy
    // with the ship stood beside the site: the effect of FoundCmd does not depend on how the ship
    // got there (the sailed canary asserts the same 29650 gold for both seats).
    const auto found = place_for_founding(w, *s, 0);
    REQUIRE(found.has_value());
    const std::int64_t gold_before = gold_of(w, 0);
    const sim::Figure* ship0 = sim::find_figure(w.figures(), sim::FigureId{1});
    std::vector<sim::ShipHold> holds_before = ship0->holds;
    REQUIRE(w.apply(*found).ok());
    const sim::Island& is = *w.islands()[s->island];
    const sim::City* city = is.city(sim::CityNo{0});
    REQUIRE(city != nullptr);
    const auto wood = *rs.types->goods_by_name("HOLZ");
    const auto tools = *rs.types->goods_by_name("WERKZEUG");
    std::uint64_t city_total = 0;
    for (const std::uint32_t v : city->stock.stock) {
        city_total += v;
    }
    MESSAGE("founded city 0 stock: total "
            << city_total << " (8.8 units), wood " << city->stock.stock.at(wood) << ", tools "
            << city->stock.stock.at(tools) << ", capacity " << city->stock.capacity);
    MESSAGE("player 0 gold: " << gold_before << " -> " << gold_of(w, 0) << " (Kontor 350)");
    std::string holds_text;
    const sim::Figure* ship_after = sim::find_figure(w.figures(), sim::FigureId{1});
    for (std::size_t i = 0; i < ship_after->holds.size(); ++i) {
        holds_text += "hold" + std::to_string(i) + " goods " +
                      std::to_string(ship_after->holds[i].goods) + " " +
                      std::to_string(holds_before[i].amount_88) + " -> " +
                      std::to_string(ship_after->holds[i].amount_88) + "; ";
    }
    MESSAGE("ship remaining cargo (raw 1/256 t): " << holds_text);
    CHECK(city_total == 0);
    CHECK(gold_of(w, 0) == 29650);
    // The cost facts (docs/re/ship-placement.md section 5) from the real registry.
    const sim::BuildingType& pionier = *rs.types->find(sim::TypeId{kPionier});
    const sim::BuildingType& handwerk = *rs.types->find(sim::TypeId{kHandwerk});
    CHECK(pionier.cost_gold == 0);
    REQUIRE(pionier.cost_goods.size() == 1);
    CHECK(pionier.cost_goods[0].goods == wood);
    CHECK(pionier.cost_goods[0].amount_88 == 3 * 256);
    CHECK(handwerk.cost_gold == 300);
    REQUIRE(handwerk.cost_goods.size() == 2);
    // Gold alone is not enough for either type: both need goods the city does not have.
    CHECK(city->stock.stock.at(wood) < pionier.cost_goods[0].amount_88);
    // A public BuildCmd on a legal site of the founded city is refused for the stock, and nothing
    // changes (the site is found by the engine's own rule on the city's own tiles).
    std::optional<Origin> own_site;
    for (const Origin& o : legal_origins(is, *rs.types, pionier)) {
        const auto [fw, fh] = sim::footprint(pionier, 0);
        bool mine = true;
        for (std::int32_t dy = 0; dy < static_cast<std::int32_t>(fh) && mine; ++dy) {
            for (std::int32_t dx = 0; dx < static_cast<std::int32_t>(fw) && mine; ++dx) {
                const sim::Slot idx = is.tile_index(o.x + dx, o.y + dy);
                mine = idx != sim::kNoSlot && is.tiles.owner(idx) == 0;
            }
        }
        if (mine) {
            own_site = o;
            break;
        }
    }
    REQUIRE_MESSAGE(own_site.has_value(),
                    "no PIONIER site of the founded city is free of scenery: the refusal must run");
    {
        const std::vector<std::uint8_t> before = w.save();
        const sim::ApplyResult r = w.apply(command_test::build(
            0, command_test::kCity0 /* island 0 */,
            {command_test::at(sim::TypeId{kPionier}, own_site->x, own_site->y)}));
        MESSAGE("BuildCmd PIONIER on the founded city at (" << own_site->x << ", " << own_site->y
                                                            << "): " << sim::reject_name(r.status));
        // The city is on island s->island; the helper addresses island 0, so only a refusal that
        // is not about the stock would be a finding. Report it, assert the island-addressed form.
        sim::CityRef ref{sim::IslandId{static_cast<std::uint32_t>(s->island)}, sim::CityNo{0}};
        const sim::ApplyResult r2 = w.apply(command_test::build(
            0, ref, {command_test::at(sim::TypeId{kPionier}, own_site->x, own_site->y)}));
        MESSAGE("same, island " << s->island << ": " << sim::reject_name(r2.status));
        CHECK(r2.status == sim::Reject::CannotAfford);
        CHECK(w.save() == before);
    }
}

namespace {

constexpr sim::CityRef kBuildCity{sim::IslandId{0}, sim::CityNo{0}};

/// One row of the evidence ledger: what a public command answered and what the world holds after.
struct Step {
    std::string label;
    sim::Reject status = sim::Reject::None;
    std::int64_t gold0 = 0, gold1 = 0;
    std::uint32_t wood = 0, tools = 0;
    std::uint64_t territory = 0;
    std::string info; // the shared-information state of city 0 and the island
    std::uint64_t hash = 0;
    friend bool operator==(const Step&, const Step&) = default;
};

std::uint64_t territory_of(const sim::World& w) {
    const sim::Island& is = *w.islands()[0];
    std::uint64_t n = 0;
    for (std::uint64_t i = 0; i < is.tiles.tile_count(); ++i) {
        n += is.tiles.owner(i) == 0 ? 1U : 0U;
    }
    return n;
}

std::string info_of(const sim::World& w) {
    const sim::Island& is = *w.islands()[0];
    std::string s = "mask=";
    const auto* const city = is.city(sim::CityNo{0});
    REQUIRE(city != nullptr);
    for (const sim::PlayerId p : city->player_mask) {
        s += std::to_string(p.v) + ",";
    }
    s += " present=";
    for (const sim::PlayerId p : is.players_present) {
        s += std::to_string(p.v) + ",";
    }
    s += " discovery1=";
    const auto* const player = w.players().find(sim::PlayerId{1});
    REQUIRE(player != nullptr);
    for (const sim::PlayerId p : player->discovery) {
        s += std::to_string(p.v) + ",";
    }
    return s;
}

/// The independent oracle of the item height: the common value of the vertex window of the real
/// terrain (vertices x..x+fw, y..y+fh, vertex (i, j) at j * width + i like the synthetic tests).
std::optional<std::uint8_t> window_level(const sim::Island& is, const sim::BuildingType& t,
                                         std::int32_t x, std::int32_t y) {
    const auto [fw, fh] = sim::footprint(t, 0);
    const std::uint32_t w = is.tiles.width();
    if (is.terrain == nullptr ||
        is.terrain->height.size() != std::uint64_t{w} * is.tiles.height()) {
        return std::nullopt;
    }
    std::optional<std::uint8_t> level;
    for (std::uint32_t j = 0; j <= fh; ++j) {
        for (std::uint32_t i = 0; i <= fw; ++i) {
            const auto vx = static_cast<std::uint32_t>(x - is.tiles.x0()) + i;
            const auto vy = static_cast<std::uint32_t>(y - is.tiles.y0()) + j;
            if (vx >= w || vy >= is.tiles.height()) {
                return std::nullopt;
            }
            const std::uint8_t v = is.terrain->height[std::uint64_t{vy} * w + vx];
            if (level.has_value() && *level != v) {
                return std::nullopt;
            }
            level = v;
        }
    }
    return level;
}

/// The building stands with `height` as byte0f and on every footprint tile (city `ref` of the
/// island slot `ref.island`).
void check_placed_in(const sim::World& w, sim::CityRef ref, const sim::BuildingType& t,
                     const Origin& o) {
    const sim::Island& is = *w.islands()[ref.island.v];
    const sim::Building* b = w.building_at(ref, o.x, o.y);
    REQUIRE(b != nullptr);
    CHECK(b->type == t.id);
    CHECK(static_cast<unsigned>(b->byte0f) == o.height);
    CHECK(o.height >= 32);
    CHECK(o.height % 16 == 0);
    const auto oracle = window_level(is, t, o.x, o.y);
    REQUIRE(oracle.has_value());
    CHECK(static_cast<unsigned>(*oracle) == o.height); // the terrain's own window value
    const auto [fw, fh] = sim::footprint(t, 0);
    for (std::uint32_t dy = 0; dy < fh; ++dy) {
        for (std::uint32_t dx = 0; dx < fw; ++dx) {
            const sim::Slot idx = is.tile_index(o.x + static_cast<std::int32_t>(dx),
                                                o.y + static_cast<std::int32_t>(dy));
            CHECK(static_cast<unsigned>(is.tiles.item_height(idx)) == o.height);
        }
    }
}

void check_placed(const sim::World& w, const sim::BuildingType& t, const Origin& o) {
    check_placed_in(w, kBuildCity, t, o);
}

std::vector<std::uint8_t> footprint_heights(const sim::World& w, const sim::BuildingType& t,
                                            const Origin& o) {
    const sim::Island& is = *w.islands()[0];
    std::vector<std::uint8_t> out;
    const auto [fw, fh] = sim::footprint(t, 0);
    for (std::uint32_t dy = 0; dy < fh; ++dy) {
        for (std::uint32_t dx = 0; dx < fw; ++dx) {
            out.push_back(is.tiles.item_height(is.tile_index(o.x + static_cast<std::int32_t>(dx),
                                                             o.y + static_cast<std::int32_t>(dy))));
        }
    }
    return out;
}

/// The whole public-command sequence of the fixture, recorded through `rec` when given, with a
/// save and load of the world after the steps selected by `reload_mask` (bit 0: after the first
/// placement, bit 1: after the demolition). `detail` adds the in-line assertions (the uninterrupted
/// run); the returned ledger is what the runs are compared by.
std::vector<Step> run_build_sequence(sim::World& w, sim::Recorder* rec, const BuildFixture& fx,
                                     const std::shared_ptr<const sim::TypeRegistry>& types,
                                     unsigned reload_mask, bool detail) {
    namespace ct = command_test;
    Driver d{&w, rec};
    const auto wood = *types->goods_by_name("HOLZ");
    const auto tools = *types->goods_by_name("WERKZEUG");
    const sim::BuildingType& pionier = *types->find(sim::TypeId{kPionier});
    const sim::BuildingType& handwerk = *types->find(sim::TypeId{kHandwerk});
    std::vector<Step> ledger;
    const auto snap = [&](const std::string& label, sim::Reject status, std::int64_t gold0,
                          std::uint32_t wood0, std::uint32_t tools0) {
        Step s;
        s.label = label;
        s.status = status;
        s.gold0 = gold_of(w, 0);
        s.gold1 = gold_of(w, 1);
        s.wood = ct::city(w, kBuildCity).stock.stock.at(wood);
        s.tools = ct::city(w, kBuildCity).stock.stock.at(tools);
        s.territory = territory_of(w);
        s.info = info_of(w);
        s.hash = w.state_hash();
        if (detail) {
            MESSAGE("ledger " << label << ": " << sim::reject_name(status) << ", gold p0 " << gold0
                              << " -> " << s.gold0 << ", p1 " << s.gold1 << ", wood(8.8) " << wood0
                              << " -> " << s.wood << ", tools(8.8) " << tools0 << " -> " << s.tools
                              << ", territory " << s.territory << ", " << s.info);
        }
        ledger.push_back(std::move(s));
    };
    const auto command = [&](const std::string& label, const sim::Command& c) {
        const std::int64_t gold0 = gold_of(w, 0);
        const std::uint32_t wood0 = ct::city(w, kBuildCity).stock.stock.at(wood);
        const std::uint32_t tools0 = ct::city(w, kBuildCity).stock.stock.at(tools);
        const std::vector<std::uint8_t> before = w.save();
        const sim::ApplyResult r = d.apply(c);
        if (r.status != sim::Reject::None) {
            REQUIRE_MESSAGE(w.save() == before, label << ": a refused command changed the world");
        }
        snap(label, r.status, gold0, wood0, tools0);
        d.checkpoint();
        return r;
    };
    const auto reload = [&] {
        auto back = sim::World::load(w.save(), types);
        REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
        REQUIRE(back.value().state_hash() == w.state_hash());
        w = std::move(back).value();
    };
    const auto pion = [&](std::uint32_t player, const Origin& o) {
        return ct::build(player, kBuildCity, {ct::at(sim::TypeId{kPionier}, o.x, o.y)});
    };
    const sim::Reject ok = sim::Reject::None;
    d.advance(100);
    // 1. the same PIONIER command on a terrain-refused site of the real island (the nearest origin
    // where placement_rule answers TerrainRefused; not asserted to be open water): the terrain
    // decides.
    const sim::ApplyResult sea = command("1 terrain-refused site", pion(0, fx.sea));
    // 2. the second player sees the island and shares information, but has no build right.
    const sim::ApplyResult foreign = command("2 second player", pion(1, fx.a));
    // 3. the legal PIONIER (3 wood).
    const sim::ApplyResult first = command("3 PIONIER at A", pion(0, fx.a));
    if (detail) {
        CHECK(sea.status == sim::Reject::TerrainRefused);
        CHECK(foreign.status == sim::Reject::NotCityOwner);
        REQUIRE(first.status == ok);
        check_placed(w, pionier, fx.a);
        CHECK(first.records.at(0).building.valid());
    }
    if ((reload_mask & 1U) != 0) {
        reload();
    }
    d.advance(100);
    d.checkpoint();
    // 4. the legal HANDWERK (300 gold, 4 wood, 3 tools) beside it.
    const sim::ApplyResult second =
        command("4 HANDWERK at B",
                ct::build(0, kBuildCity, {ct::at(sim::TypeId{kHandwerk}, fx.b.x, fx.b.y)}));
    if (detail) {
        REQUIRE(second.status == ok);
        check_placed(w, handwerk, fx.b);
    }
    d.advance(100);
    // 5. demolish the PIONIER: the clear arm keeps the height byte.
    std::vector<std::uint8_t> heights_before;
    if (detail) {
        heights_before = footprint_heights(w, pionier, fx.a);
    }
    const sim::ApplyResult gone =
        command("5 demolish A", ct::demolish(0, kBuildCity, {{fx.a.x, fx.a.y}}));
    if (detail) {
        REQUIRE(gone.status == ok);
        CHECK(w.building_at(kBuildCity, fx.a.x, fx.a.y) == nullptr);
        CHECK(footprint_heights(w, pionier, fx.a) == heights_before);
    }
    if ((reload_mask & 2U) != 0) {
        reload();
    }
    d.advance(100);
    d.checkpoint();
    // 6. a second PIONIER on the freed site.
    const sim::ApplyResult again = command("6 PIONIER at A again", pion(0, fx.a));
    if (detail) {
        REQUIRE(again.status == ok);
        check_placed(w, pionier, fx.a);
    }
    d.advance(500);
    d.checkpoint();
    return ledger;
}

} // namespace

TEST_CASE(
    "build on the real terrain: stocked fixture, public commands, save and reload, and replay") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    namespace ct = command_test;
    RealStart rs = real_start(*dir);
    auto loaded = sl::load_ship_start(rs.manifest, *dir, rs.types, {}, rs.rules);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    const sim::World start = std::move(loaded).value();
    constexpr std::int64_t kGold = 5000;
    constexpr std::uint32_t kWoodT = 20, kToolsT = 10;
    auto found = find_build_fixture(start, rs.types, rs.manifest.edge_w, rs.manifest.edge_h, kGold,
                                    kWoodT, kToolsT);
    REQUIRE_MESSAGE(found.has_value(),
                    "no real island offers PIONIER, HANDWERK and a terrain-refused site");
    const BuildFixture fx = std::move(*found);
    MESSAGE("real island slot " << fx.island << ": PIONIER A (" << fx.a.x << ", " << fx.a.y
                                << ") level " << unsigned{fx.a.height} << "; HANDWERK B (" << fx.b.x
                                << ", " << fx.b.y << ") level " << unsigned{fx.b.height}
                                << "; terrain-refused site (" << fx.sea.x << ", " << fx.sea.y
                                << ")");
    const auto wood = *rs.types->goods_by_name("HOLZ");
    const auto tools = *rs.types->goods_by_name("WERKZEUG");
    const sim::BuildingType& pionier = *rs.types->find(sim::TypeId{kPionier});
    const sim::BuildingType& handwerk = *rs.types->find(sim::TypeId{kHandwerk});
    // The charges the ledger below must equal are the registry's own (8.8 tons).
    REQUIRE(pionier.cost_goods.size() == 1);
    REQUIRE(handwerk.cost_goods.size() == 2);
    CHECK(pionier.cost_gold == 0);
    CHECK(pionier.cost_goods[0].goods == wood);
    CHECK(pionier.cost_goods[0].amount_88 == 3 * 256);
    CHECK(handwerk.cost_gold == 300);
    CHECK(handwerk.cost_goods[0].goods == tools);
    CHECK(handwerk.cost_goods[0].amount_88 == 3 * 256);
    CHECK(handwerk.cost_goods[1].goods == wood);
    CHECK(handwerk.cost_goods[1].amount_88 == 4 * 256);

    auto created = sim::World::create(fx.spec, rs.types, {}, fx.rules);
    REQUIRE_MESSAGE(created.ok(), (created.ok() ? std::string() : created.error()));
    const sim::World pristine = std::move(created).value();
    REQUIRE_MESSAGE(pristine.check_invariants().empty(), pristine.check_invariants());
    // The declared fixture state, before any command.
    CHECK(gold_of(pristine, 0) == kGold);
    CHECK(ct::city(pristine, kBuildCity).stock.stock.at(wood) == kWoodT * sim::kTon);
    CHECK(ct::city(pristine, kBuildCity).stock.stock.at(tools) == kToolsT * sim::kTon);
    MESSAGE("territory tiles of city 0: " << territory_of(pristine) << "; " << info_of(pristine));

    // ---- the uninterrupted, recorded run -------------------------------------------------------
    sim::World w = pristine;
    auto started = sim::Recorder::start(w);
    REQUIRE_MESSAGE(started.ok(), (started.ok() ? std::string() : started.error()));
    sim::Recorder rec = std::move(started).value();
    const std::vector<Step> ledger = run_build_sequence(w, &rec, fx, rs.types, 0, true);
    REQUIRE(ledger.size() == 6);
    REQUIRE_MESSAGE(w.check_invariants().empty(), w.check_invariants());
    const std::uint32_t t = sim::kTon;
    // charges: exact per command (8.8 tons; the fixture started at kGold / kWoodT / kToolsT).
    CHECK(ledger[0].gold0 == kGold); // sea refusal: nothing charged
    CHECK(ledger[0].wood == kWoodT * t);
    CHECK(ledger[0].tools == kToolsT * t);
    CHECK(ledger[1].hash == ledger[0].hash); // foreign refusal: world unchanged
    CHECK(ledger[2].gold0 == kGold);         // PIONIER: no gold, 3 wood
    CHECK(ledger[2].wood == (kWoodT - 3) * t);
    CHECK(ledger[2].tools == kToolsT * t);
    CHECK(ledger[3].gold0 == kGold - 300); // HANDWERK: 300 gold, 4 wood, 3 tools
    CHECK(ledger[3].wood == (kWoodT - 7) * t);
    CHECK(ledger[3].tools == (kToolsT - 3) * t);
    CHECK(ledger[4].gold0 == ledger[3].gold0); // demolition: no refund, no charge
    CHECK(ledger[4].wood == ledger[3].wood);
    CHECK(ledger[4].tools == ledger[3].tools);
    CHECK(ledger[5].gold0 == kGold - 300); // second PIONIER: 3 wood again
    CHECK(ledger[5].wood == (kWoodT - 10) * t);
    CHECK(ledger[5].tools == (kToolsT - 3) * t);
    for (const Step& s : ledger) {
        CHECK(s.gold1 == kGold);                   // the second player never paid
        CHECK(s.territory == ledger[0].territory); // building does not move the territory
        CHECK(s.info == ledger[0].info);           // shared information unchanged
    }
    CHECK(ledger[0].info == "mask=0,1, present=0,1, discovery1=");
    // Second PIONIER sits where the first stood: same level again.
    check_placed(w, pionier, fx.a);

    // ---- save and load after the placement and after the demolition: the same outcome -----------
    {
        sim::World w2 = pristine;
        const std::vector<Step> reloaded = run_build_sequence(w2, nullptr, fx, rs.types, 3, false);
        CHECK(reloaded == ledger);
        CHECK(w2.state_hash() == w.state_hash());
        CHECK(w2.save() == w.save());
        sim::World w3 = pristine; // and a reload after the placement only
        CHECK(run_build_sequence(w3, nullptr, fx, rs.types, 1, false) == ledger);
        CHECK(w3.state_hash() == w.state_hash());
    }

    // ---- replay from the log: in this process, through bytes, and a changed command -------------
    auto bytes = sim::write_log(rec.log());
    REQUIRE(bytes.ok());
    {
        const std::string e = export_log_if_requested(std::getenv("ANNO_BUILD_LOG"), bytes.value());
        REQUIRE_MESSAGE(e.empty(), e);
    }
    auto read = sim::read_log(bytes.value());
    REQUIRE(read.ok());
    auto fresh = sim::World::load(read.value().initial_save, rs.types, {});
    REQUIRE_MESSAGE(fresh.ok(), (fresh.ok() ? std::string() : fresh.error()));
    CHECK(fresh.value().state_hash() == pristine.state_hash());
    auto outcome = sim::replay(fresh.value(), read.value());
    REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
    CHECK(outcome.value().reproduced());
    CHECK(fresh.value().state_hash() == w.state_hash());
    {
        sim::ReplayLog tampered = read.value();
        bool edited = false;
        for (sim::ReplayEntry& e : tampered.entries) {
            for (sim::Command& c : e.commands) {
                auto* b = std::get_if<sim::BuildCmd>(&c.body);
                if (b != nullptr && !edited && c.issuer.v == 0 && b->placements.at(0).x == fx.a.x &&
                    b->placements.at(0).y == fx.a.y) {
                    b->placements[0].x += 1; // the first legal PIONIER one tile east
                    edited = true;
                }
            }
        }
        REQUIRE(edited);
        if (const char* bad_path = std::getenv("ANNO_BUILD_LOG_TAMPERED");
            bad_path != nullptr && *bad_path) {
            const auto bad_bytes = sim::write_log(tampered);
            REQUIRE(bad_bytes.ok());
            const std::string e = export_log_if_requested(bad_path, bad_bytes.value());
            REQUIRE_MESSAGE(e.empty(), e);
        }
        auto fresh2 = sim::World::load(tampered.initial_save, rs.types, {});
        REQUIRE(fresh2.ok());
        auto bad = sim::replay(fresh2.value(), tampered);
        CHECK((!bad.ok() || !bad.value().reproduced()));
    }
}

// ---- MAP-CARGO-1 U4: the real opening loop without fixture grants -------------------------------
// The sailed canary's two participants (seat 0 and seat 2, different owners) on the UNCHANGED
// candidate and loadout: sail, arrive, found (FoundCmd), move normally only if the unloading range
// of docs/sim/ship-cargo.md section 5 requires it, UnloadCmd one command per hold, then ordinary
// BuildCmds on the founded city's own territory. Every change of the executed world goes through
// public commands (Recorder::submit / World::apply) and World::advance; the test only reads and
// SEARCHES copies. Nothing is granted: no stock, no gold, no relocation, no free type.

namespace {

using Holds = std::vector<sim::ShipHold>;

const sim::Figure& ship_of(const sim::World& w, std::uint64_t id) {
    const sim::Figure* f = sim::find_figure(w.figures(), sim::FigureId{id});
    REQUIRE(f != nullptr);
    return *f;
}

std::optional<sim::CityRef> city_of_player(const sim::World& w, std::uint32_t player) {
    for (std::size_t i = 0; i < w.islands().size(); ++i) {
        const auto& isl = w.islands()[i];
        if (!isl) {
            continue;
        }
        for (std::size_t j = 0; j < isl->cities.size(); ++j) {
            const auto& c = isl->cities[j];
            if (c.has_value() && c->owner == sim::PlayerId{player}) {
                return sim::CityRef{sim::IslandId{static_cast<std::uint32_t>(i)},
                                    sim::CityNo{static_cast<std::uint32_t>(j)}};
            }
        }
    }
    return std::nullopt;
}

std::size_t city_count(const sim::World& w) {
    std::size_t n = 0;
    for (const auto& isl : w.islands()) {
        for (const auto& c : isl ? isl->cities : decltype(isl->cities){}) {
            n += c.has_value() ? 1U : 0U;
        }
    }
    return n;
}

/// One row of the comparison ledger: what a public command answered and what the world holds
/// after it, for both seats. Two runs of the same commands must produce equal rows.
struct OStep {
    std::string label;
    sim::Reject status = sim::Reject::None;
    std::uint64_t moved_88 = 0;
    std::int64_t gold0 = 0, gold2 = 0;
    std::vector<std::uint32_t> stock0, stock2;
    Holds holds0, holds2;
    std::uint64_t hash = 0;
    friend bool operator==(const OStep&, const OStep&) = default;
};

/// What the accounting of one seat needs (all goods quantities in 1/256 ton).
struct Seat {
    std::uint32_t player = 0;
    std::uint64_t ship = 0;
    std::optional<sim::CityRef> city;
    sim::BuildingId warehouse;
    Holds start_holds;
    std::int64_t gold_start = 0, gold_spent = 0, gold_observed = 0, last_gold = 0;
    std::vector<std::int64_t> found_cost, credits, build_cost, observed, loaded;
    std::vector<std::uint32_t> last_stock;
    Origin a, b;                    // PIONIER site, HANDWERK site (terrain level in .height)
    std::size_t road_buildings = 0; // MAP-ROAD-1: buildings and next serial before the roads
    std::uint32_t road_serial = 0;
};

struct OpeningOut {
    std::vector<OStep> ledger;
    std::array<sim::BuildingId, 2> warehouses{};
    /// MAP-ROAD-1: per seat, the two adjacent road tiles of the first drag (the second one is
    /// removed and rebuilt) and another legal tile for the changed-command control.
    std::array<std::array<Origin, 2>, 2> road_pair{};
    std::array<Origin, 2> road_alt{};
};

/// Two edge-adjacent tiles (x, y), (x + 1, y) where a land road is legal by the rule and the city
/// owns the tile, nearest to the warehouse (Chebyshev distance, then row-major), and one more
/// legal tile elsewhere.
struct RoadSite {
    Origin first, second, alt;
};
std::optional<RoadSite> road_site(const sim::World& w, sim::CityRef ref,
                                  const sim::BuildingType& road, const sim::Building& wh) {
    const sim::Island& is = *w.islands()[ref.island.v];
    std::vector<Origin> legal;
    // The sites follow the world's own (enhanced) road choices, read from its Rules.
    sim::PlacementParams params;
    params.road_require_land_level = w.rules().road_require_land_level;
    params.road_preserve_nonroad_pieces = w.rules().road_preserve_nonroad_pieces;
    for (const Origin& o : legal_origins(is, w.types(), road, params)) {
        const sim::Slot idx = is.tile_index(o.x, o.y);
        if (idx != sim::kNoSlot && is.tiles.owner(idx) == ref.city.v) {
            legal.push_back(o);
        }
    }
    std::stable_sort(legal.begin(), legal.end(), [&](const Origin& a, const Origin& b) {
        return std::max(std::abs(a.x - wh.x), std::abs(a.y - wh.y)) <
               std::max(std::abs(b.x - wh.x), std::abs(b.y - wh.y));
    });
    const auto has = [&](std::int32_t x, std::int32_t y) {
        return std::any_of(legal.begin(), legal.end(),
                           [&](const Origin& o) { return o.x == x && o.y == y; });
    };
    for (const Origin& o : legal) {
        if (!has(o.x + 1, o.y)) {
            continue;
        }
        for (const Origin& alt : legal) {
            if ((alt.x == o.x || alt.x == o.x + 1) && alt.y == o.y) {
                continue;
            }
            return RoadSite{
                o,
                *std::find_if(legal.begin(), legal.end(),
                              [&](const Origin& q) { return q.x == o.x + 1 && q.y == o.y; }),
                alt};
        }
    }
    return std::nullopt;
}

/// Orders the legs (MoveCmd) and advances until each ship reported ShipArrived. No founding.
SailOutcome sail_phase(Driver& d, sim::World& w, const std::vector<SailLeg>& legs, bool timing) {
    SailOutcome out;
    for (const SailLeg& leg : legs) {
        REQUIRE(d.apply(sail_command(leg)).ok());
    }
    std::vector<sim::FixedPos> last;
    std::vector<std::uint16_t> last_visit;
    for (const SailLeg& leg : legs) {
        const sim::Figure& f = ship_of(w, leg.ship);
        last.push_back(f.pos);
        last_visit.push_back(f.nav->last);
    }
    std::size_t arrived = 0;
    (void)w.drain_events();
    for (std::uint64_t adv = 1; adv <= 40000 && arrived < legs.size(); ++adv) {
        const auto t0 = std::chrono::steady_clock::now();
        d.advance(10);
        if (timing) {
            out.advance_us.push_back(
                std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0)
                    .count());
        }
        for (const sim::SimEvent& e : w.drain_events()) {
            if (e.kind == sim::SimEventKind::ShipArrived) {
                ++arrived;
            } else if (e.kind == sim::SimEventKind::ShipRouteFailed) {
                ++out.failures;
                arrived = legs.size();
            }
        }
        for (std::size_t i = 0; i < legs.size(); ++i) {
            const sim::Figure& f = ship_of(w, legs[i].ship);
            const std::int32_t step =
                std::max(std::abs(f.pos.x - last[i].x), std::abs(f.pos.y - last[i].y));
            const auto ticks = static_cast<std::uint16_t>(f.nav->last - last_visit[i]);
            out.worst_step_q8 = std::max(out.worst_step_q8, step);
            out.worst_excess_q8 = std::max<std::int64_t>(
                out.worst_excess_q8, step - (std::int64_t{ticks} * 18432 / 10000 + 1));
            last[i] = f.pos;
            last_visit[i] = f.nav->last;
        }
        if (adv % 2000 == 0) {
            d.checkpoint();
        }
        out.arrival_advances = adv;
    }
    out.all_arrived = out.failures == 0 && arrived == legs.size();
    d.checkpoint();
    return out;
}

/// The legal origins of `type` on the city's own tiles (every footprint tile owned by the city:
/// the tile owner value is the city number),
/// nearest to the warehouse first (Chebyshev distance of the origins, then row-major); `avoid`
/// excludes sites that touch an earlier choice.
std::optional<Origin>
own_site(const sim::World& w, sim::CityRef ref, const sim::BuildingType& type,
         const sim::Building& wh,
         const std::vector<std::pair<Origin, const sim::BuildingType*>>& avoid) {
    const sim::Island& is = *w.islands()[ref.island.v];
    const auto [fw, fh] = sim::footprint(type, 0);
    std::optional<Origin> best;
    int best_d = 1 << 30;
    for (const Origin& o : legal_origins(is, w.types(), type)) {
        bool mine = true;
        for (std::int32_t dy = 0; dy < static_cast<std::int32_t>(fh) && mine; ++dy) {
            for (std::int32_t dx = 0; dx < static_cast<std::int32_t>(fw) && mine; ++dx) {
                const sim::Slot idx = is.tile_index(o.x + dx, o.y + dy);
                mine = idx != sim::kNoSlot && is.tiles.owner(idx) == ref.city.v;
            }
        }
        for (const auto& [ao, at] : avoid) {
            const auto [aw, ah] = sim::footprint(*at, 0);
            mine = mine && apart(o, fw, fh, ao, aw, ah);
        }
        if (!mine) {
            continue;
        }
        const int d = std::max(std::abs(o.x - wh.x), std::abs(o.y - wh.y));
        if (d < best_d) {
            best_d = d;
            best = o;
        }
    }
    return best;
}

std::int64_t aboard_of(const Holds& holds, sim::GoodsId g) {
    std::int64_t n = 0;
    for (const sim::ShipHold& h : holds) {
        n += h.goods == g ? static_cast<std::int64_t>(h.amount_88) : 0;
    }
    return n;
}

/// The whole opening of both seats from the moment both ships have arrived: found, (move,)
/// refusals of the rival, build-before-unload refusals, one unload per hold, build. `reload_mask`
/// saves and loads the world after the founding (bit 0), after the unloading (bit 1), before the
/// first resupply load (bit 2), between the two seats' resupply loads (bit 3) and after them (bit
/// 4).
OpeningOut run_opening(sim::World& w, sim::Recorder* rec, const std::vector<SailLeg>& legs,
                       const std::shared_ptr<const sim::TypeRegistry>& types, unsigned reload_mask,
                       bool detail) {
    namespace ct = command_test;
    Driver d{&w, rec};
    REQUIRE(legs.size() == 2);
    const sim::BuildingType& kontor = *types->find(sim::TypeId{1302});
    const sim::BuildingType& pionier = *types->find(sim::TypeId{kPionier});
    const sim::BuildingType& handwerk = *types->find(sim::TypeId{kHandwerk});
    OpeningOut out;
    std::vector<Seat> seats(2);
    for (std::size_t i = 0; i < 2; ++i) {
        seats[i].player = legs[i].player;
        seats[i].ship = legs[i].ship;
        seats[i].start_holds = ship_of(w, legs[i].ship).holds;
        seats[i].gold_start = gold_of(w, legs[i].player);
        seats[i].last_gold = seats[i].gold_start;
    }
    const auto stock_of = [&](const Seat& s) {
        return s.city ? ct::city(w, *s.city).stock.stock : std::vector<std::uint32_t>{};
    };
    const auto observe = [&] { // simulation changes between commands: counted, never ignored
        for (Seat& s : seats) {
            s.observed.resize(sim::kOriginalWarehouseGoods, 0);
            const std::vector<std::uint32_t> now = stock_of(s);
            for (std::size_t g = 0; g < now.size() && g < s.last_stock.size(); ++g) {
                s.observed[g] +=
                    static_cast<std::int64_t>(now[g]) - static_cast<std::int64_t>(s.last_stock[g]);
            }
            s.gold_observed += gold_of(w, s.player) - s.last_gold;
        }
    };
    const auto rebase = [&] {
        for (Seat& s : seats) {
            s.last_stock = stock_of(s);
            s.last_gold = gold_of(w, s.player);
        }
    };
    const auto snap = [&](const std::string& label, const sim::ApplyResult& r) {
        OStep st;
        st.label = label;
        st.status = r.status;
        st.moved_88 = r.records.empty() ? 0 : r.records[0].transferred_88;
        st.gold0 = gold_of(w, seats[0].player);
        st.gold2 = gold_of(w, seats[1].player);
        st.stock0 = stock_of(seats[0]);
        st.stock2 = stock_of(seats[1]);
        st.holds0 = ship_of(w, seats[0].ship).holds;
        st.holds2 = ship_of(w, seats[1].ship).holds;
        st.hash = w.state_hash();
        if (detail) {
            MESSAGE("opening " << label << ": " << sim::reject_name(r.status) << ", moved(8.8) "
                               << st.moved_88 << ", gold " << st.gold0 << "/" << st.gold2);
        }
        out.ledger.push_back(std::move(st));
    };
    const auto command = [&](const std::string& label, const sim::Command& c) {
        observe();
        const std::vector<std::uint8_t> before = w.save();
        const sim::ApplyResult r = d.apply(c);
        if (r.status != sim::Reject::None) {
            REQUIRE_MESSAGE(w.save() == before, label << ": a refused command changed the world");
        }
        rebase();
        snap(label, r);
        d.checkpoint();
        return r;
    };
    const auto settle = [&](std::uint32_t ms) {
        d.advance(ms);
        d.checkpoint();
    };
    const auto reload = [&] {
        auto back = sim::World::load(w.save(), types);
        REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
        REQUIRE(back.value().state_hash() == w.state_hash());
        w = std::move(back).value();
    };
    const sim::Reject ok = sim::Reject::None;

    // ---- 1. found the first warehouse of each seat --------------------------------------------
    for (std::size_t i = 0; i < 2; ++i) {
        Seat& s = seats[i];
        const Holds before = ship_of(w, s.ship).holds;
        const sim::ApplyResult r =
            command("found seat " + std::to_string(s.player), found_command(legs[i]));
        REQUIRE_MESSAGE(r.status == ok, "founding was refused: " << sim::reject_name(r.status));
        s.warehouse = r.records.at(0).building;
        s.city = city_of_player(w, s.player);
        REQUIRE(s.city.has_value());
        out.warehouses[i] = s.warehouse;
        s.last_stock = stock_of(s);
        const Holds after = ship_of(w, s.ship).holds;
        REQUIRE(before.size() == after.size());
        s.found_cost.assign(sim::kOriginalWarehouseGoods, 0);
        for (std::size_t h = 0; h < before.size(); ++h) {
            REQUIRE(before[h].amount_88 >= after[h].amount_88);
            if (before[h].goods != 0) {
                s.found_cost.at(before[h].goods) +=
                    static_cast<std::int64_t>(before[h].amount_88 - after[h].amount_88);
            }
        }
        s.gold_spent += kontor.cost_gold;
        if (detail) {
            // The founding consumed exactly what the registry says, from the holds, and the gold.
            std::vector<std::int64_t> want(sim::kOriginalWarehouseGoods, 0);
            for (const auto& cg : kontor.cost_goods) {
                want.at(cg.goods) += cg.amount_88;
            }
            CHECK(s.found_cost == want);
            CHECK(gold_of(w, s.player) == s.gold_start - kontor.cost_gold);
            const auto stock = stock_of(s);
            CHECK(std::all_of(stock.begin(), stock.end(), [](std::uint32_t v) { return v == 0; }));
        }
        settle(100);
    }
    if ((reload_mask & 1U) != 0) {
        reload();
    }
    REQUIRE(city_count(w) == 2);

    // ---- 2. is the arrival position within the unloading range? ---------------------------------
    const auto first_hold = [&](const Seat& s) {
        const Holds& h = ship_of(w, s.ship).holds;
        for (std::uint32_t i = 0; i < h.size(); ++i) {
            if (h[i].goods != 0 && h[i].amount_88 != 0) {
                return i;
            }
        }
        FAIL("a ship without cargo");
        return 0U;
    };
    const auto unload_cmd = [&](const Seat& s, sim::BuildingId wh, std::uint32_t hold,
                                std::uint32_t tons) {
        return sim::Command{sim::PlayerId{s.player},
                            sim::UnloadCmd{sim::FigureId{s.ship}, wh, hold,
                                           ship_of(w, s.ship).holds.at(hold).goods, tons}};
    };
    // lan5-ship-a needs no move: a probe UnloadCmd on a copy at the arrival position is accepted
    // for both seats. A move-then-unload path is NOT covered by this test; if the probe is ever
    // refused the case fails with the numbers instead of moving the ship.
    for (std::size_t i = 0; i < 2; ++i) {
        const Seat& s = seats[i];
        const sim::Command probe = unload_cmd(s, s.warehouse, first_hold(s), 1);
        sim::World trial = w;
        const sim::ApplyResult pr = trial.apply(probe);
        const sim::Figure& sh = ship_of(w, s.ship);
        const sim::Building& kb = *w.find_building(s.warehouse);
        if (detail) {
            MESSAGE("seat " << s.player << ": ship tile (" << (sh.pos.x >> 8) << ", "
                            << (sh.pos.y >> 8) << "), warehouse origin (" << kb.x << ", " << kb.y
                            << "), probe unload at the arrival position: "
                            << sim::reject_name(pr.status));
        }
        REQUIRE_MESSAGE(pr.status == ok,
                        "seat " << s.player << ": the unload at the arrival position is refused ("
                                << sim::reject_name(pr.status) << "): ship tile ("
                                << (sh.pos.x >> 8) << ", " << (sh.pos.y >> 8)
                                << "), Kontor origin (" << kb.x << ", " << kb.y << ")");
    }

    // ---- 3. the rival's attempts (information is shared, access is not) ----------------------
    const sim::BuildingType* types_to_build[2] = {&handwerk, &pionier};
    for (std::size_t i = 0; i < 2; ++i) { // legal sites, decided on the live world by the rule
        Seat& s = seats[i];
        const sim::Building& wh = *w.find_building(s.warehouse);
        const auto b = own_site(w, *s.city, handwerk, wh, {});
        REQUIRE_MESSAGE(b.has_value(), "seat " << s.player << ": no HANDWERK site on its own land");
        const auto a = own_site(w, *s.city, pionier, wh, {{*b, &handwerk}});
        REQUIRE_MESSAGE(a.has_value(), "seat " << s.player << ": no PIONIER site on its own land");
        s.a = *a;
        s.b = *b;
        if (detail) {
            MESSAGE("seat " << s.player << " sites: HANDWERK (" << b->x << ", " << b->y
                            << ") level " << unsigned{b->height} << ", PIONIER (" << a->x << ", "
                            << a->y << ") level " << unsigned{a->height});
        }
    }
    const auto build_cmd = [&](const Seat& s, const sim::BuildingType& t, const Origin& o,
                               std::uint32_t issuer) {
        return ct::build(issuer, *s.city, {ct::at(t.id, o.x, o.y)});
    };
    for (std::size_t i = 0; i < 2; ++i) {
        const std::size_t j = 1 - i;
        const Seat& me = seats[i];
        const Seat& other = seats[j];
        const std::string tag =
            "seat " + std::to_string(me.player) + " vs seat " + std::to_string(other.player);
        const auto r1 = command(tag + " unload into the rival warehouse",
                                unload_cmd(me, other.warehouse, first_hold(me), 1));
        const auto r2 = command(tag + " build in the rival city",
                                build_cmd(other, pionier, other.a, me.player));
        sim::UnloadCmd stolen{sim::FigureId{other.ship}, me.warehouse, 0, 0, 1};
        stolen.goods = ship_of(w, other.ship).holds.at(first_hold(other)).goods;
        stolen.hold = first_hold(other);
        const auto r3 =
            command(tag + " unload the rival ship", sim::Command{sim::PlayerId{me.player}, stolen});
        if (detail) {
            CHECK(r1.status == sim::Reject::NotCityOwner);
            CHECK(r2.status == sim::Reject::NotCityOwner);
            CHECK(r3.status == sim::Reject::NotShipOwner);
        }
    }

    // ---- 4. the same builds BEFORE unloading: cargo aboard is not city stock ------------------
    for (std::size_t i = 0; i < 2; ++i) {
        const Seat& s = seats[i];
        const auto rh = command("seat " + std::to_string(s.player) + " HANDWERK before unloading",
                                build_cmd(s, handwerk, s.b, s.player));
        const auto rp = command("seat " + std::to_string(s.player) + " PIONIER before unloading",
                                build_cmd(s, pionier, s.a, s.player));
        if (detail) {
            CHECK(rh.status == sim::Reject::CannotAfford);
            CHECK(rp.status == sim::Reject::CannotAfford);
        }
    }
    settle(100);

    // ---- 5. unload: one command per hold, whole tons, honouring the store limit --------------
    for (std::size_t i = 0; i < 2; ++i) {
        Seat& s = seats[i];
        s.credits.assign(sim::kOriginalWarehouseGoods, 0);
        const std::size_t n_holds = ship_of(w, s.ship).holds.size();
        for (std::uint32_t h = 0; h < n_holds; ++h) {
            const sim::ShipHold hold = ship_of(w, s.ship).holds[h];
            if (hold.goods == 0 || hold.amount_88 == 0) {
                continue;
            }
            const std::uint32_t tons = static_cast<std::uint32_t>((hold.amount_88 + 255) / 256);
            const sim::City& city = ct::city(w, *s.city);
            const std::uint64_t cap = city.stock.capacity, have = city.stock.stock.at(hold.goods);
            const std::uint64_t free_88 = cap > have ? cap - have : 0;
            const std::uint64_t want = sim::ShipCargo::amount_88(tons, hold.amount_88, free_88);
            const sim::ApplyResult r =
                command("seat " + std::to_string(s.player) + " unload hold " + std::to_string(h),
                        unload_cmd(s, s.warehouse, h, tons));
            if (detail) {
                MESSAGE("seat " << s.player << " hold " << h << ": goods " << hold.goods << ", "
                                << hold.amount_88 << " aboard, asked " << tons << " t, store "
                                << have << "/" << cap << " -> moved "
                                << (r.records.empty() ? 0 : r.records[0].transferred_88)
                                << " (expected " << want << ")");
            }
            if (want == 0) {
                if (detail) {
                    CHECK(r.status == sim::Reject::StockFull);
                }
                continue;
            }
            REQUIRE_MESSAGE(r.status == ok, "unload refused: " << sim::reject_name(r.status));
            const std::uint64_t moved = r.records.at(0).transferred_88;
            if (detail) {
                CHECK(moved == want);
                CHECK(ship_of(w, s.ship).holds[h].amount_88 == hold.amount_88 - moved);
                CHECK(ct::city(w, *s.city).stock.stock.at(hold.goods) == have + moved);
            }
            s.credits.at(hold.goods) += static_cast<std::int64_t>(moved);
            settle(100); // the simulation runs between the commands; its changes are observed
        }
        // The store is full for what is still aboard: a repeat is refused (B15), nothing moves.
        for (std::uint32_t h = 0; h < n_holds; ++h) {
            const sim::ShipHold hold = ship_of(w, s.ship).holds[h];
            if (hold.goods != 0 && hold.amount_88 != 0) {
                const auto r = command("seat " + std::to_string(s.player) + " repeat hold " +
                                           std::to_string(h) + " into the full store",
                                       unload_cmd(s, s.warehouse, h, 1));
                if (detail) {
                    CHECK(r.status == sim::Reject::StockFull);
                }
            }
        }
    }
    if ((reload_mask & 2U) != 0) {
        reload();
    }

    // ---- 6. the ordinary buildings: the commands refused before now succeed (identical commands)
    // ------------------
    for (std::size_t i = 0; i < 2; ++i) {
        Seat& s = seats[i];
        s.build_cost.assign(sim::kOriginalWarehouseGoods, 0);
        for (std::size_t k = 0; k < 2; ++k) {
            const sim::BuildingType& t = *types_to_build[k];
            const Origin& o = k == 0 ? s.b : s.a;
            observe();
            const std::vector<std::uint32_t> stock0 = stock_of(s);
            const std::int64_t gold0 = gold_of(w, s.player);
            const sim::ApplyResult r =
                command("seat " + std::to_string(s.player) + " build " + std::to_string(t.id.v),
                        build_cmd(s, t, o, s.player));
            REQUIRE_MESSAGE(r.status == ok, "seat " << s.player << " build " << t.id.v
                                                    << " refused: " << sim::reject_name(r.status));
            for (const auto& cg : t.cost_goods) {
                s.build_cost.at(cg.goods) += cg.amount_88;
            }
            s.gold_spent += t.cost_gold;
            if (detail) {
                check_placed_in(w, *s.city, t, o);
                const std::vector<std::uint32_t> stock1 = stock_of(s);
                std::vector<std::int64_t> spent(stock0.size(), 0);
                for (const auto& cg : t.cost_goods) {
                    spent.at(cg.goods) += cg.amount_88;
                }
                for (std::size_t g = 0; g < stock0.size(); ++g) {
                    CHECK(static_cast<std::int64_t>(stock0[g]) -
                              static_cast<std::int64_t>(stock1[g]) ==
                          spent[g]);
                }
                CHECK(gold0 - gold_of(w, s.player) == t.cost_gold);
            }
            settle(100);
        }
    }
    // ---- 6a. ordinary land roads (MAP-ROAD-1 R4.3) --------------------------------------------
    // After the unloading and the builds, with the original supplies and public commands only: a
    // two-tile road drag on the city's own tiles, a rival's attempt, the removal of one tile, a
    // repeated removal, and the rebuild. A road is a tile object: the gold falls by exactly the
    // registry cost, no building and no serial appears, the tile record carries the height of
    // the terrain window (recomputed here from the vertex grid, not by the rule under test) and the
    // walk code of the type's HEAD pair (0, 3) = 0, and every later quiet window must stay quiet.
    const sim::BuildingType& road = *types->find(sim::TypeId{kRoadDirt});
    REQUIRE(sim::is_land_road(road));
    REQUIRE(road.head_raw.size() > 0x30);
    REQUIRE(road.head_raw[0x2f] == 0);
    REQUIRE(road.head_raw[0x30] == 3);
    REQUIRE(road.cost_goods.empty());
    for (std::size_t i = 0; i < 2; ++i) {
        Seat& s = seats[i];
        const Seat& other = seats[1 - i];
        const sim::Building& wh = *w.find_building(s.warehouse);
        const auto site = road_site(w, *s.city, road, wh);
        REQUIRE_MESSAGE(site.has_value(), "seat " << s.player << ": no road site on its own land");
        out.road_pair[i] = {site->first, site->second};
        out.road_alt[i] = site->alt;
        const std::string tag = "seat " + std::to_string(s.player);
        const sim::Island& is = *w.islands()[s.city->island.v];
        const auto road_tile = [&](const Origin& o) {
            const sim::Slot idx = is.tile_index(o.x, o.y);
            REQUIRE(idx != sim::kNoSlot);
            return idx;
        };
        const auto expect_road = [&](const Origin& o) { // the tile as the placement left it
            if (!detail) {
                return;
            }
            const sim::VertexGrid grid = sim::VertexGrid::of(is);
            REQUIRE(grid.ok());
            unsigned lo = 0xff;
            for (const auto& [dx, dy] : {std::pair{0, 0}, {1, 0}, {0, 1}, {1, 1}}) {
                const auto v = grid.at(o.x + dx, o.y + dy);
                REQUIRE(v.has_value());
                lo = std::min<unsigned>(lo, *v);
            }
            const sim::Slot idx = road_tile(o);
            CHECK(is.tiles.object(idx) == kRoadDirt);
            CHECK(static_cast<unsigned>(is.tiles.item_height(idx)) == (lo & 0xf0U));
            CHECK(is.tiles.item_height(idx) == o.height);
            CHECK(is.tiles.walk_code(idx) == 0);
            CHECK(is.tiles.walk_mask(idx) == 0);
            CHECK(is.tiles.foot_dx(idx) == 0);
            CHECK(is.tiles.foot_dy(idx) == 0);
            CHECK(is.cache.by_id.size() == s.road_buildings);
            CHECK(is.next_serial == s.road_serial);
        };
        s.road_buildings = is.cache.by_id.size();
        s.road_serial = is.next_serial;
        const std::int64_t gold0 = gold_of(w, s.player);
        const auto drag = command(tag + " build a road drag",
                                  ct::build(s.player, *s.city,
                                            {ct::at(road.id, site->first.x, site->first.y),
                                             ct::at(road.id, site->second.x, site->second.y)}));
        REQUIRE_MESSAGE(drag.status == ok,
                        tag << " road drag refused: " << sim::reject_name(drag.status));
        REQUIRE(drag.records.size() == 2);
        for (const sim::RecordResult& rr : drag.records) {
            REQUIRE(rr.status == ok);
            if (detail) {
                CHECK(rr.tile_object);
                CHECK_FALSE(rr.building.valid());
            }
        }
        s.gold_spent += 2 * road.cost_gold;
        if (detail) {
            CHECK(gold0 - gold_of(w, s.player) == 2 * road.cost_gold);
            MESSAGE(tag << " roads at (" << site->first.x << ", " << site->first.y << ") and ("
                        << site->second.x << ", " << site->second.y << "), level "
                        << unsigned{site->first.height} << ", " << road.cost_gold << " gold each");
        }
        expect_road(site->first);
        expect_road(site->second);
        settle(100);
        // The rival seat's attempt on this city: refused, nothing changes (the command layer
        // checks the world is byte-identical).
        const auto rival =
            command(tag + " road in the rival city",
                    ct::build(s.player, *other.city, {ct::at(road.id, site->alt.x, site->alt.y)}));
        if (detail) {
            CHECK(rival.status == sim::Reject::NotCityOwner);
        }
        // Remove the second tile: the tile object only, no refund.
        const std::int64_t gold1 = gold_of(w, s.player);
        const sim::Command rm{sim::PlayerId{s.player},
                              sim::DemolishCmd{*s.city, {{site->second.x, site->second.y}}}};
        const auto removed = command(tag + " remove a road tile", rm);
        REQUIRE(removed.status == ok);
        REQUIRE(removed.records.size() == 1);
        REQUIRE(removed.records[0].status == ok);
        if (detail) {
            const sim::Slot idx = road_tile(site->second);
            CHECK(is.tiles.object(idx) == 0);
            CHECK(is.tiles.walk_code(idx) == 2);
            CHECK(is.tiles.walk_mask(idx) == 0);
            CHECK(gold_of(w, s.player) == gold1); // no refund
            CHECK(is.cache.by_id.size() == s.road_buildings);
            CHECK(is.next_serial == s.road_serial);
        }
        expect_road(site->first); // the neighbour tile is untouched
        // A second removal of the same tile finds nothing and changes nothing.
        {
            const std::vector<std::uint8_t> before = w.save();
            const auto again = command(tag + " remove the same road tile again", rm);
            REQUIRE(again.records.size() == 1);
            if (detail) {
                CHECK(again.records[0].status == sim::Reject::NoBuildingAtTile);
            }
            REQUIRE_MESSAGE(w.save() == before, tag << ": a repeated removal changed the world");
        }
        // Rebuild it: charged again, the tile is a road again.
        const std::int64_t gold2 = gold_of(w, s.player);
        const auto rebuilt = command(
            tag + " rebuild the road tile",
            ct::build(s.player, *s.city, {ct::at(road.id, site->second.x, site->second.y)}));
        REQUIRE(rebuilt.status == ok);
        s.gold_spent += road.cost_gold;
        if (detail) {
            CHECK(gold2 - gold_of(w, s.player) == road.cost_gold);
        }
        expect_road(site->second);
        settle(100);
    }
    settle(500);
    observe();
    rebase();

    // ---- 6b. resupply: legitimate remaining city goods back aboard through LoadCmd -------------
    // Public commands only (encode -> decode -> Recorder/World apply). No grant, no setter, no
    // relocation: what leaves a city is exactly what the command reports, and the windows between
    // the commands must show no economy change at all (asserted to be exactly 0, not absorbed).
    const sim::GoodsId wood_id = *types->goods_by_name("HOLZ");
    const sim::GoodsId tools_id = *types->goods_by_name("WERKZEUG");
    sim::GoodsId other_id = 0; // a goods no hold and no store of the opening has
    for (std::size_t g = 1; g < sim::kOriginalWarehouseGoods && other_id == 0; ++g) {
        if (g != wood_id && g != tools_id && g != seats[0].start_holds.at(1).goods) {
            other_id = static_cast<sim::GoodsId>(g);
        }
    }
    REQUIRE(other_id != 0);
    const sim::GoodsId food_id = seats[0].start_holds.at(1).goods;
    const auto ship_facts = [&](const Seat& s) {
        const sim::FigureType* ft = types->find_figure_type(ship_of(w, s.ship).kind);
        REQUIRE(ft != nullptr);
        REQUIRE(ft->nav.has_value());
        return std::pair<std::uint32_t, std::uint32_t>{ft->capacity_88, ft->nav->hold_count_a};
    };
    // ship + city per goods, in a wide accumulator (1/256 ton)
    const auto combined = [&](const sim::World& world, const Seat& s) {
        std::vector<std::int64_t> c(sim::kOriginalWarehouseGoods, 0);
        for (const sim::ShipHold& h : ship_of(world, s.ship).holds) {
            if (h.goods != 0) {
                c.at(h.goods) += static_cast<std::int64_t>(h.amount_88);
            }
        }
        const auto& st = ct::city(world, *s.city).stock.stock;
        for (std::size_t g = 0; g < c.size() && g < st.size(); ++g) {
            c[g] += static_cast<std::int64_t>(st[g]);
        }
        return c;
    };
    // The window between two commands changed nothing: stock, gold and holds are exactly as the
    // last command left them.
    Holds quiet_holds[2];
    const auto quiet_mark = [&] {
        rebase();
        for (std::size_t i = 0; i < 2; ++i) {
            quiet_holds[i] = ship_of(w, seats[i].ship).holds;
        }
    };
    std::int64_t quiet_windows = 0;
    const auto quiet_check = [&](const std::string& what) {
        for (std::size_t i = 0; i < 2; ++i) {
            ++quiet_windows;
            if (detail) {
                CHECK_MESSAGE(stock_of(seats[i]) == seats[i].last_stock,
                              what << ": seat " << seats[i].player << " stock changed");
                CHECK_MESSAGE(gold_of(w, seats[i].player) == seats[i].last_gold,
                              what << ": seat " << seats[i].player << " gold changed");
                CHECK_MESSAGE(ship_of(w, seats[i].ship).holds == quiet_holds[i],
                              what << ": seat " << seats[i].player << " holds changed");
            }
        }
    };
    const auto load_cmd = [&](std::uint32_t issuer, const Seat& ship_of_seat, sim::BuildingId wh,
                              std::uint32_t hold, sim::GoodsId goods, std::uint32_t tons) {
        const sim::Command c{sim::PlayerId{issuer},
                             sim::LoadCmd{sim::FigureId{ship_of_seat.ship}, wh, hold, goods, tons}};
        const sim::TurnEntry in{c, 9};
        auto bytes = sim::encode_turn_entry(in);
        REQUIRE(bytes.ok());
        auto back = sim::decode_turn_entry(bytes.value());
        REQUIRE(back.ok());
        REQUIRE(back.value() == in);
        return back.value().command;
    };
    const auto settle_quiet = [&](const std::string& what) {
        settle(100);
        quiet_check(what);
    };
    // Preconditions of the unchanged state, with the numbers of this run.
    std::int64_t rival_refusals = 0;
    for (std::size_t i = 0; i < 2; ++i) {
        const Seat& s = seats[i];
        const auto [cap, n0] = ship_facts(s);
        const Holds& h = ship_of(w, s.ship).holds;
        const auto& st = ct::city(w, *s.city).stock;
        if (detail) {
            MESSAGE("seat " << s.player << " before the resupply: capacity_88 " << cap << ", n0 "
                            << n0 << ", holds " << h[0].goods << ":" << h[0].amount_88 << " "
                            << h[1].goods << ":" << h[1].amount_88 << " " << h[2].goods << ":"
                            << h[2].amount_88 << " " << h[3].goods << ":" << h[3].amount_88
                            << ", store wood " << st.stock.at(wood_id) << " food "
                            << st.stock.at(food_id) << " tools " << st.stock.at(tools_id) << " of "
                            << st.capacity);
            CHECK(cap == 12800);
            CHECK(n0 == 4);
            REQUIRE(h.size() == 4);
            // the unload emptied three holds (goods cleared) and left 6400 tools in the last
            for (std::size_t k = 0; k < 3; ++k) {
                CHECK(h[k].goods == 0);
                CHECK(h[k].amount_88 == 0);
            }
            CHECK(h[3].goods == tools_id);
            CHECK(h[3].amount_88 == 6400);
            CHECK(st.stock.at(wood_id) == 7936);
            CHECK(st.stock.at(food_id) == 2560);
            CHECK(st.stock.at(tools_id) == 12032);
            for (const sim::GoodsId g : {wood_id, food_id, tools_id}) {
                CHECK(st.stock.at(g) % 256 == 0); // whole tons: the floor is not exercised here
            }
            CHECK(st.stock.at(other_id) == 0);
            // the first part of the ledger is exactly accounted: nothing unexplained so far
            for (std::size_t g = 0; g < sim::kOriginalWarehouseGoods; ++g) {
                CHECK_MESSAGE(s.observed.at(g) == 0, "seat " << s.player << " goods " << g
                                                             << " observed " << s.observed.at(g));
            }
            CHECK(s.gold_observed == 0);
        }
    }
    for (Seat& s : seats) {
        s.loaded.assign(sim::kOriginalWarehouseGoods, 0);
    }
    quiet_mark();
    if ((reload_mask & 4U) != 0) {
        reload();
    }
    // The rivals: the other seat's ship against my warehouse and my ship against its warehouse.
    for (std::size_t i = 0; i < 2; ++i) {
        const Seat& me = seats[i];
        const Seat& other = seats[1 - i];
        const std::string tag =
            "seat " + std::to_string(me.player) + " vs seat " + std::to_string(other.player);
        const auto r1 = command(tag + " load from the rival warehouse",
                                load_cmd(me.player, me, other.warehouse, 0, wood_id, 1));
        const auto r2 = command(tag + " load onto the rival ship",
                                load_cmd(me.player, other, me.warehouse, 0, wood_id, 1));
        rival_refusals += 2;
        if (detail) {
            CHECK(r1.status == sim::Reject::NotCityOwner);
            CHECK(r2.status == sim::Reject::NotShipOwner);
            CHECK(r1.records.empty());
            CHECK(r2.records.empty());
        }
    }
    quiet_mark();
    // Load then unload part of it, no Advance in between: the combined goods are conserved. A
    // world copy; the executed world is untouched (its hash is compared after).
    if (detail) {
        const std::uint64_t hash_before = w.state_hash();
        const Seat& s = seats[0];
        sim::World trial = w;
        const std::vector<std::int64_t> c0 = combined(trial, s);
        const sim::Command lc = load_cmd(s.player, s, s.warehouse, 0, wood_id, 12);
        const sim::ApplyResult lr = trial.apply(lc);
        REQUIRE(lr.status == ok);
        CHECK(lr.records.at(0).transferred_88 == 3072);
        CHECK(combined(trial, s) == c0);
        const sim::Command uc{sim::PlayerId{s.player},
                              sim::UnloadCmd{sim::FigureId{s.ship}, s.warehouse, 0, wood_id, 5}};
        const sim::ApplyResult ur = trial.apply(uc);
        REQUIRE(ur.status == ok);
        CHECK(ur.records.at(0).transferred_88 == 1280);
        CHECK(combined(trial, s) == c0);
        CHECK(ship_of(trial, s.ship).holds[0].amount_88 == 3072 - 1280);
        CHECK(ct::city(trial, *s.city).stock.stock.at(wood_id) == 7936 - 3072 + 1280);
        CHECK(w.state_hash() == hash_before);
    }

    // One seat's resupply: all numbers hand-derived from the preconditions above.
    struct Planned {
        const char* what;
        std::uint32_t hold;
        sim::GoodsId goods;
        std::uint32_t tons;
        sim::Reject status;
        std::uint64_t amount;   // the expected transfer (1/256 t)
        std::uint32_t lands_in; // the hold that changes
    };
    const auto resupply_seat = [&](Seat& s) {
        const Planned plan[] = {
            // a newly emptied hold takes the goods: 12 t of 31 t of wood, not limited
            {"wood into the emptied hold 0 (12 t)", 0, wood_id, 12, ok, 3072, 0},
            // top-up of the hold that kept 6400 tools: 40 t asked (10240), room 6400, store 47 t
            {"tools top up hold 3 (40 t asked, room limited)", 3, tools_id, 40, ok, 6400, 3},
            // the rest of the wood: 100 t asked, hold room 9728, store 4864 -> the store limits
            {"wood into hold 0 again (100 t asked, store limited)", 0, wood_id, 100, ok, 4864, 0},
            // the remaining tools go to the first empty hold (hold 3 is full): hold 1, newly
            // emptied
            {"tools into the first empty hold (40 t asked, store limited)", 3, tools_id, 40, ok,
             5632, 1},
            // all the food into the next emptied hold; the hint (2) is an empty hold
            {"food into the emptied hold 2 (100 t asked, store limited)", 2, food_id, 100, ok, 2560,
             2},
            // nothing of wood is left in the store; the hold has room: StockShort
            {"wood with an empty store", 0, wood_id, 1, sim::Reject::StockShort, 0, 0},
            // all four holds are in use by other goods: HoldsFull (before the store is looked at)
            {"another goods with every hold in use", 0, other_id, 1, sim::Reject::HoldsFull, 0, 0},
        };
        for (const Planned& p : plan) {
            const Holds h0 = ship_of(w, s.ship).holds;
            const std::vector<std::uint32_t> st0 = stock_of(s);
            const std::uint32_t cap = ship_facts(s).first;
            // the formula: min(request, room of the chosen hold, whole tons in the store)
            const std::uint64_t held = h0[p.lands_in].amount_88;
            const std::uint64_t room = cap - held;
            const std::uint64_t avail = std::uint64_t{st0.at(p.goods)} / 256 * 256;
            const std::uint64_t formula = std::min({std::uint64_t{p.tons} * 256, room, avail});
            if (detail && p.status == ok) {
                CHECK(formula == p.amount);
            }
            const std::string label = "seat " + std::to_string(s.player) + " " + p.what;
            const sim::ApplyResult r =
                command(label, load_cmd(s.player, s, s.warehouse, p.hold, p.goods, p.tons));
            if (detail) {
                MESSAGE(label << ": " << sim::reject_name(r.status) << ", moved(8.8) "
                              << (r.records.empty() ? 0 : r.records[0].transferred_88) << ", store "
                              << st0.at(p.goods) << " -> " << stock_of(s).at(p.goods));
            }
            REQUIRE_MESSAGE(r.status == p.status, label << ": " << sim::reject_name(r.status));
            if (p.status != ok) {
                if (detail) {
                    CHECK(r.records.empty());
                    CHECK(ship_of(w, s.ship).holds == h0);
                    CHECK(stock_of(s) == st0);
                }
                continue;
            }
            const std::uint64_t moved = r.records.at(0).transferred_88;
            s.loaded.at(p.goods) += static_cast<std::int64_t>(moved);
            if (detail) {
                CHECK(moved == p.amount);
                const Holds h1 = ship_of(w, s.ship).holds;
                for (std::uint32_t k = 0; k < h1.size(); ++k) {
                    if (k == p.lands_in) {
                        CHECK(h1[k].goods == p.goods);
                        CHECK(h1[k].amount_88 == h0[k].amount_88 + moved);
                        CHECK(h1[k].amount_88 <= cap);
                    } else {
                        CHECK(h1[k] == h0[k]);
                    }
                }
                const std::vector<std::uint32_t> st1 = stock_of(s);
                for (std::size_t g = 0; g < st1.size(); ++g) {
                    CHECK(static_cast<std::int64_t>(st0[g]) - static_cast<std::int64_t>(st1[g]) ==
                          (g == p.goods ? static_cast<std::int64_t>(moved) : 0));
                }
            }
            quiet_mark();
            settle_quiet(label);
        }
    };
    resupply_seat(seats[0]);
    if ((reload_mask & 8U) != 0) {
        reload();
    }
    quiet_mark();
    resupply_seat(seats[1]);
    if ((reload_mask & 16U) != 0) {
        reload();
    }
    settle(500);
    observe();
    rebase();
    if (detail) {
        MESSAGE("resupply: " << rival_refusals << " rival refusals, " << quiet_windows
                             << " quiet seat-windows asserted unchanged");
    }

    // ---- 7. the ledger ------------------------------------------------------------------------
    if (detail) {
        for (const Seat& s : seats) {
            const Holds now = ship_of(w, s.ship).holds;
            const auto stock = stock_of(s);
            std::int64_t observed_total = 0;
            for (std::size_t g = 0; g < sim::kOriginalWarehouseGoods; ++g) {
                const auto gid = static_cast<sim::GoodsId>(g);
                // starting cargo - founding consumption - unloads + loads = cargo still aboard
                CHECK(aboard_of(s.start_holds, gid) - s.found_cost.at(g) - s.credits.at(g) +
                          s.loaded.at(g) ==
                      aboard_of(now, gid));
                // city credits - building consumption - loads + observed changes = stock
                CHECK(s.credits.at(g) - s.build_cost.at(g) - s.loaded.at(g) + s.observed.at(g) ==
                      static_cast<std::int64_t>(stock.at(g)));
                // ship + city: the loader's quantities change only by the founding and building
                // consumption; nothing unexplained (the observed term is asserted to be 0)
                CHECK(aboard_of(now, gid) + static_cast<std::int64_t>(stock.at(g)) ==
                      aboard_of(s.start_holds, gid) - s.found_cost.at(g) - s.build_cost.at(g));
                CHECK_MESSAGE(s.observed.at(g) == 0, "seat " << s.player << " goods " << g
                                                             << " unexplained "
                                                             << s.observed.at(g));
                observed_total += s.observed.at(g) < 0 ? -s.observed.at(g) : s.observed.at(g);
            }
            CHECK(gold_of(w, s.player) == s.gold_start - s.gold_spent + s.gold_observed);
            CHECK(s.gold_observed == 0);
            MESSAGE("seat " << s.player << " ledger: gold " << s.gold_start << " - spent "
                            << s.gold_spent << " + observed " << s.gold_observed << " = "
                            << gold_of(w, s.player) << "; |observed stock changes| "
                            << observed_total);
            std::string text;
            for (std::size_t g = 0; g < sim::kOriginalWarehouseGoods; ++g) {
                if (s.credits.at(g) != 0 || s.build_cost.at(g) != 0 || s.found_cost.at(g) != 0 ||
                    stock.at(g) != 0 || aboard_of(now, static_cast<sim::GoodsId>(g)) != 0) {
                    text += "goods " + std::to_string(g) + ": start aboard " +
                            std::to_string(aboard_of(s.start_holds, static_cast<sim::GoodsId>(g))) +
                            " - founding " + std::to_string(s.found_cost.at(g)) + " - unloaded " +
                            std::to_string(s.credits.at(g)) + " + loaded " +
                            std::to_string(s.loaded.at(g)) + " = aboard " +
                            std::to_string(aboard_of(now, static_cast<sim::GoodsId>(g))) +
                            "; credits " + std::to_string(s.credits.at(g)) + " - loaded " +
                            std::to_string(s.loaded.at(g)) + " - build " +
                            std::to_string(s.build_cost.at(g)) + " + observed " +
                            std::to_string(s.observed.at(g)) + " = store " +
                            std::to_string(stock.at(g)) + " | ";
                }
            }
            MESSAGE("seat " << s.player << " goods ledger (8.8): " << text);
        }
        REQUIRE_MESSAGE(w.check_invariants().empty(), w.check_invariants());
    }
    return out;
}

std::vector<SailLeg> plan_opening_legs(const RealStart& rs, const sim::World& pristine) {
    const sim::BuildingType& kontor = *rs.types->find(sim::TypeId{1302});
    std::vector<SailLeg> legs(2);
    legs[0].player = 0;
    legs[0].ship = 1; // seat 0
    legs[1].player = 2;
    legs[1].ship = 7; // seat 2
    const auto first_site = find_legal_site(pristine, kontor, pristine.config().neutral_owner);
    REQUIRE(first_site.has_value());
    legs[0].site = *first_site;
    const auto g0 = pick_water_goal(pristine, pristine, legs[0]);
    REQUIRE_MESSAGE(g0.has_value(), "no reachable water tile beside the first site");
    legs[0].goal_x = g0->first;
    legs[0].goal_y = g0->second;
    sim::World trial = pristine;
    sim::WorldTestAccess::relocate(trial, sim::FigureId{1},
                                   sim::FixedPos{g0->first * 256 + 128, g0->second * 256 + 128});
    REQUIRE(trial.apply(found_command(legs[0])).ok());
    const auto second_site = find_legal_site(trial, kontor, trial.config().neutral_owner);
    REQUIRE(second_site.has_value());
    legs[1].site = *second_site;
    const auto g1 = pick_water_goal(pristine, trial, legs[1]);
    REQUIRE_MESSAGE(g1.has_value(), "no reachable water tile beside the second site");
    legs[1].goal_x = g1->first;
    legs[1].goal_y = g1->second;
    return legs;
}

} // namespace

TEST_CASE("sailed founding canary: the opening loop unloads the unchanged loadout and builds, "
          "two seats, reload and replay") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        // Without the game files this case returns early with a MESSAGE: not an executed pass.
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    RealStart rs = real_start(*dir);
    // The enhanced session of the road canary: both explicit road choices on (MAP-ROAD-CLOSE-1;
    // the defaults of Rules are the faithful ones). They are hashed and saved, so the recording,
    // the reloads and the fresh-process replay all carry them.
    rs.rules.road_require_land_level = true;
    rs.rules.road_preserve_nonroad_pieces = true;
    auto loaded = sl::load_ship_start(rs.manifest, *dir, rs.types, {}, rs.rules);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    const sim::World pristine = std::move(loaded).value();
    REQUIRE(sl::expedition_baseline_problem(pristine).empty());
    CHECK(pristine.rules().road_require_land_level);
    CHECK(pristine.rules().road_preserve_nonroad_pieces);
    const std::uint64_t pristine_hash = pristine.state_hash();

    // ---- the registry facts the ledger is derived from ---------------------------------------
    const sim::BuildingType& kontor = *rs.types->find(sim::TypeId{1302});
    const sim::BuildingType& pionier = *rs.types->find(sim::TypeId{kPionier});
    const sim::BuildingType& handwerk = *rs.types->find(sim::TypeId{kHandwerk});
    const auto wood = *rs.types->goods_by_name("HOLZ");
    const auto tools = *rs.types->goods_by_name("WERKZEUG");
    CHECK(kontor.cost_gold == 350);
    REQUIRE(pionier.cost_goods.size() == 1);
    CHECK(pionier.cost_goods[0].goods == wood);
    CHECK(pionier.cost_goods[0].amount_88 == 3 * 256);
    CHECK(pionier.cost_gold == 0);
    REQUIRE(handwerk.cost_goods.size() == 2);
    CHECK(handwerk.cost_gold == 300);
    CHECK(handwerk.cost_goods[0].goods == tools);
    CHECK(handwerk.cost_goods[0].amount_88 == 3 * 256);
    CHECK(handwerk.cost_goods[1].goods == wood);
    CHECK(handwerk.cost_goods[1].amount_88 == 4 * 256);
    // Both are affordable from the unchanged loadout: the ship carries wood and tools beyond the
    // founding cost, and 29650 gold remain.
    MESSAGE("registry: Kontor 1302 gold "
            << kontor.cost_gold << ", PIONIER 501 gold " << pionier.cost_gold << " wood(8.8) "
            << pionier.cost_goods[0].amount_88 << ", HANDWERK 1001 gold " << handwerk.cost_gold);

    // ---- the starting state: every hold, the gold, no city anywhere --------------------------
    CHECK(city_count(pristine) == 0);
    for (const std::uint64_t id : {1ULL, 4ULL, 7ULL, 10ULL, 13ULL}) {
        std::string text;
        for (const sim::ShipHold& h : ship_of(pristine, id).holds) {
            text += std::to_string(h.goods) + ":" + std::to_string(h.amount_88) + " ";
        }
        const std::uint32_t seat = static_cast<std::uint32_t>((id - 1) / 3);
        MESSAGE("start ship " << id << " (seat " << seat << ", owner "
                              << ship_of(pristine, id).owner.v << ") holds goods:amount(8.8) "
                              << text << "| gold " << gold_of(pristine, seat));
        CHECK(gold_of(pristine, seat) == 30000);
    }

    const std::vector<SailLeg> legs = plan_opening_legs(rs, pristine);

    // ---- the recorded run: sail, then the whole opening --------------------------------------
    sim::World w = pristine;
    auto started = sim::Recorder::start(w);
    REQUIRE(started.ok());
    sim::Recorder rec = std::move(started).value();
    Driver d{&w, &rec};
    const SailOutcome voyage = sail_phase(d, w, legs, false);
    REQUIRE_MESSAGE(voyage.failures == 0, "a route failed");
    REQUIRE(voyage.all_arrived);
    CHECK(voyage.worst_excess_q8 <= 0); // no teleport
    for (const SailLeg& leg : legs) {
        const sim::Figure& s = ship_of(w, leg.ship);
        CHECK(s.pos.x / 256 == leg.goal_x);
        CHECK(s.pos.y / 256 == leg.goal_y);
        CHECK(ship_of(w, leg.ship).holds == ship_of(pristine, leg.ship).holds); // cargo untouched
        CHECK(gold_of(w, leg.player) == 30000);
    }
    CHECK(city_count(w) == 0);
    const sim::World at_arrival = w; // the same voyage; the variants below start from it
    const OpeningOut main_run = run_opening(w, &rec, legs, rs.types, 0, true);
    REQUIRE_MESSAGE(w.check_invariants().empty(), w.check_invariants());
    const std::vector<std::uint8_t> final_save = w.save();
    const std::uint64_t final_hash = w.state_hash();
    // 2 foundings, 6 refusals of the rival, 4 builds refused before unloading, 2 x (4 unloads and
    // 1 refused repeat), 4 builds, 4 refused rival loads, 2 x (5 loads and 2 refused loads).
    // 2 x 5 road commands: drag, rival attempt, removal, repeated removal, rebuild.
    CHECK(main_run.ledger.size() == 26 + 4 + 14 + 10);

    // ---- save and load between founding and unloading, and after unloading: same outcomes -----
    // Three variants, each combining boundaries: after founding AND after unloading AND before the
    // first load (7: the combined cut of the older CARGO-1 variant, restored by MAP-CARGO-3),
    // after unloading + between the seats' loads (10), after founding + after the loads (17);
    // every boundary is reloaded in at least one variant.
    for (const unsigned mask : {7U, 10U, 17U}) {
        sim::World v = at_arrival;
        const OpeningOut var = run_opening(v, nullptr, legs, rs.types, mask, false);
        CHECK(var.ledger == main_run.ledger);
        CHECK(v.state_hash() == final_hash);
        CHECK(v.save() == final_save);
    }

    // ---- replay: this process, through bytes, and a changed UnloadCmd -------------------------
    auto bytes = sim::write_log(rec.log());
    REQUIRE(bytes.ok());
    {
        const std::string e =
            export_log_if_requested(std::getenv("ANNO_OPENING_LOG"), bytes.value());
        REQUIRE_MESSAGE(e.empty(), e);
    }
    auto read = sim::read_log(bytes.value());
    REQUIRE(read.ok());
    auto fresh = sim::World::load(read.value().initial_save, rs.types, {});
    REQUIRE_MESSAGE(fresh.ok(), (fresh.ok() ? std::string() : fresh.error()));
    CHECK(fresh.value().state_hash() == pristine_hash);
    auto outcome = sim::replay(fresh.value(), read.value());
    REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
    CHECK(outcome.value().reproduced());
    CHECK(fresh.value().state_hash() == final_hash);
    CHECK(fresh.value().save() == final_save);
    {
        // Two in-process tampered replays. (1) The first resupply wood load asks one ton fewer
        // (12 -> 11): exported (ANNO_OPENING_LOG_TAMPERED) for the fresh-process CLI run. (2) One
        // unload asks one ton fewer. Each must diverge.
        sim::ReplayLog tampered = read.value();
        bool edited = false;
        for (sim::ReplayEntry& e : tampered.entries) {
            for (sim::Command& c : e.commands) {
                auto* u = std::get_if<sim::LoadCmd>(&c.body);
                if (u != nullptr && !edited && c.issuer.v == 0 && u->ship == sim::FigureId{1} &&
                    u->warehouse == main_run.warehouses[0] && u->tons == 12) {
                    u->tons -= 1;
                    edited = true;
                }
            }
        }
        REQUIRE(edited);
        {
            sim::ReplayLog tampered_unload = read.value();
            bool unedited = false;
            for (sim::ReplayEntry& e : tampered_unload.entries) {
                for (sim::Command& c : e.commands) {
                    auto* u = std::get_if<sim::UnloadCmd>(&c.body);
                    if (u != nullptr && !unedited && c.issuer.v == 0 &&
                        u->ship == sim::FigureId{1} && u->warehouse == main_run.warehouses[0] &&
                        u->tons > 1) {
                        u->tons -= 1;
                        unedited = true;
                    }
                }
            }
            REQUIRE(unedited);
            auto fresh3 = sim::World::load(tampered_unload.initial_save, rs.types, {});
            REQUIRE(fresh3.ok());
            auto bad3 = sim::replay(fresh3.value(), tampered_unload);
            CHECK((!bad3.ok() || !bad3.value().reproduced()));
        }
        {
            // (3) MAP-ROAD-1: the rebuild of seat 0's removed road tile goes to another LEGAL tile
            // (chosen by the rule before the drag, so the command executes and the world differs):
            // exported (ANNO_OPENING_LOG_ROAD_TAMPERED) for the fresh-process CLI run.
            sim::ReplayLog tampered_road = read.value();
            bool moved = false;
            for (sim::ReplayEntry& e : tampered_road.entries) {
                for (sim::Command& c : e.commands) {
                    auto* b = std::get_if<sim::BuildCmd>(&c.body);
                    if (b != nullptr && !moved && c.issuer.v == 0 && b->placements.size() == 1 &&
                        b->placements[0].type.v == kRoadDirt &&
                        b->placements[0].x == main_run.road_pair[0][1].x &&
                        b->placements[0].y == main_run.road_pair[0][1].y) {
                        b->placements[0].x = main_run.road_alt[0].x;
                        b->placements[0].y = main_run.road_alt[0].y;
                        moved = true;
                    }
                }
            }
            REQUIRE(moved);
            if (const char* road_path = std::getenv("ANNO_OPENING_LOG_ROAD_TAMPERED");
                road_path != nullptr && *road_path) {
                const auto road_bytes = sim::write_log(tampered_road);
                REQUIRE(road_bytes.ok());
                const std::string e = export_log_if_requested(road_path, road_bytes.value());
                REQUIRE_MESSAGE(e.empty(), e);
            }
            auto fresh4 = sim::World::load(tampered_road.initial_save, rs.types, {});
            REQUIRE(fresh4.ok());
            auto bad4 = sim::replay(fresh4.value(), tampered_road);
            CHECK((!bad4.ok() || !bad4.value().reproduced()));
        }
        if (const char* bad_path = std::getenv("ANNO_OPENING_LOG_TAMPERED");
            bad_path != nullptr && *bad_path) {
            const auto bad_bytes = sim::write_log(tampered);
            REQUIRE(bad_bytes.ok());
            const std::string e = export_log_if_requested(bad_path, bad_bytes.value());
            REQUIRE_MESSAGE(e.empty(), e);
        }
        auto fresh2 = sim::World::load(tampered.initial_save, rs.types, {});
        REQUIRE(fresh2.ok());
        auto bad = sim::replay(fresh2.value(), tampered);
        CHECK((!bad.ok() || !bad.value().reproduced()));
    }
}

// ---------------------------------------------------------------------------------------------
// MAP-ECON-1 E5: the forester opening (docs/sim/forestry.md 7, TASKS E5)
// ---------------------------------------------------------------------------------------------
namespace {

constexpr std::uint32_t kForester801 = 801; // a fixture identifier: the engine reads its data

/// A forester site, the road tiles that join it to the warehouse, and its tree count.
struct ForestPlan {
    Origin site;
    std::vector<Origin> road;
    std::uint32_t trees = 0;
};

/// The cuttable mature trees (the registry rule: WARE[0] is the worker's cargo goods and the cut's
/// predicate holds) in the forester's candidate square (search_area), outside its footprint.
std::uint32_t trees_in_square(const sim::World& w, const sim::Island& is,
                              const sim::BuildingType& f, const Origin& o, sim::GoodsId cargo) {
    sim::Building b;
    b.type = f.id;
    b.x = o.x;
    b.y = o.y;
    const sim::TileRect sq = sim::search_area(b, f).square();
    std::uint32_t n = 0;
    for (std::int32_t y = sq.y0; y < sq.y0 + static_cast<std::int32_t>(sq.h); ++y) {
        for (std::int32_t x = sq.x0; x < sq.x0 + static_cast<std::int32_t>(sq.w); ++x) {
            const sim::Slot idx = is.tile_index(x, y);
            if (idx == sim::kNoSlot || (x >= o.x && x < o.x + 3 && y >= o.y && y < o.y + 3)) {
                continue;
            }
            n +=
                sim::harvest_candidate_type(w.types(), is.tiles.object(idx), cargo, true) ? 1U : 0U;
        }
    }
    return n;
}

/// A 4-connected path of legal own road tiles from beside the warehouse to beside `site` (both
/// footprints are walk code 4, which the cart's layer admits; the road tiles have code 0). Breadth
/// first in row-major order: the shortest such path, deterministic.
std::optional<std::vector<Origin>> road_path(const sim::World& w, sim::CityRef ref,
                                             const sim::BuildingType& road, const sim::Building& wh,
                                             const sim::BuildingType& kontor, const Origin& site) {
    const sim::Island& is = *w.islands()[ref.island.v];
    std::map<std::pair<std::int32_t, std::int32_t>, Origin> legal;
    for (const Origin& o : legal_origins(is, w.types(), road)) {
        const sim::Slot idx = is.tile_index(o.x, o.y);
        const bool in_site = o.x >= site.x && o.x < site.x + 3 && o.y >= site.y && o.y < site.y + 3;
        if (idx != sim::kNoSlot && is.tiles.owner(idx) == ref.city.v && !in_site) {
            legal[{o.x, o.y}] = o;
        }
    }
    const auto [kw, kh] = sim::footprint(kontor, wh.rotation);
    const auto touches = [](std::int32_t x, std::int32_t y, std::int32_t bx, std::int32_t by,
                            std::int32_t bw, std::int32_t bh) {
        const bool inside_x = x >= bx && x < bx + bw, inside_y = y >= by && y < by + bh;
        return (inside_x && (y == by - 1 || y == by + bh)) ||
               (inside_y && (x == bx - 1 || x == bx + bw));
    };
    std::map<std::pair<std::int32_t, std::int32_t>, std::pair<std::int32_t, std::int32_t>> from;
    std::vector<std::pair<std::int32_t, std::int32_t>> queue;
    for (const auto& [xy, o] : legal) {
        if (touches(xy.first, xy.second, wh.x, wh.y, static_cast<std::int32_t>(kw),
                    static_cast<std::int32_t>(kh))) {
            from[xy] = xy;
            queue.push_back(xy);
        }
    }
    for (std::size_t q = 0; q < queue.size(); ++q) {
        const auto cur = queue[q];
        if (touches(cur.first, cur.second, site.x, site.y, 3, 3)) {
            std::vector<Origin> path;
            for (auto at = cur;; at = from[at]) {
                path.push_back(legal.at(at));
                if (from[at] == at) {
                    break;
                }
            }
            return path;
        }
        for (const auto& [dx, dy] : {std::pair{0, -1}, {-1, 0}, {1, 0}, {0, 1}}) {
            const std::pair<std::int32_t, std::int32_t> nb{cur.first + dx, cur.second + dy};
            if (legal.contains(nb) && !from.contains(nb)) {
                from[nb] = cur;
                queue.push_back(nb);
            }
        }
    }
    return std::nullopt;
}

sim::Command road_drag(std::uint32_t player, sim::CityRef city, const std::vector<Origin>& path) {
    std::vector<sim::PlacementWire> ps;
    for (const Origin& o : path) {
        ps.push_back(command_test::at(sim::TypeId{kRoadDirt}, o.x, o.y));
    }
    return command_test::build(player, city, std::move(ps));
}

} // namespace

TEST_CASE("forester opening canary: one seat sails, founds, unloads, builds a forester and roads, "
          "harvests existing trees and delivers new wood, reload and replay") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set (a required-asset skip, not a pass)");
        return;
    }
    namespace ct = command_test;
    RealStart rs =
        real_start(*dir); // the unchanged candidate, the original loadout, faithful Rules
    auto loaded = sl::load_ship_start(rs.manifest, *dir, rs.types, {}, rs.rules);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    const sim::World pristine = std::move(loaded).value();
    REQUIRE(sl::expedition_baseline_problem(pristine).empty());
    CHECK_FALSE(pristine.rules().forestry_claim_hygiene);
    CHECK_FALSE(pristine.rules().forestry_skip_uncuttable);
    const sim::BuildingType& kontor = *rs.types->find(sim::TypeId{1302});
    const sim::BuildingType& forester = *rs.types->find(sim::TypeId{kForester801});
    const sim::BuildingType& road = *rs.types->find(sim::TypeId{kRoadDirt});
    const sim::GoodsId wood = *rs.types->goods_by_name("HOLZ");
    const sim::GoodsId baum = *rs.types->goods_by_name("BAUM");
    // The registry facts the ledger uses (docs/re/forestry.md 1.1): no wood in either cost.
    for (const auto& cg : forester.cost_goods) {
        CHECK(cg.goods != wood);
    }
    CHECK(road.cost_goods.empty());
    REQUIRE(forester.input_goods == std::vector<sim::GoodsId>{baum});

    // ---- the recorded run: seat 0 sails, founds, unloads every hold --------------------------
    std::vector<SailLeg> legs = plan_opening_legs(rs, pristine);
    legs.resize(1); // one seat: seat 0's ship 1
    const std::uint64_t ship_id = legs[0].ship;
    const std::uint32_t player = legs[0].player;
    sim::World w = pristine;
    auto started = sim::Recorder::start(w);
    REQUIRE(started.ok());
    sim::Recorder rec = std::move(started).value();
    Driver d{&w, &rec};
    const SailOutcome voyage = sail_phase(d, w, legs, false);
    REQUIRE(voyage.all_arrived);
    const Holds start_holds = ship_of(w, ship_id).holds;
    const auto aboard_wood = [&](const sim::World& world) {
        return aboard_of(ship_of(world, ship_id).holds, wood);
    };
    const std::int64_t ship_wood_start = aboard_wood(w);
    CHECK(ship_wood_start == 12800); // 50 t of wood aboard (the manifest, docs/sim/ship-cargo.md)
    const sim::ApplyResult found = d.apply(found_command(legs[0]));
    REQUIRE_MESSAGE(found.status == sim::Reject::None, sim::reject_name(found.status));
    d.checkpoint();
    const sim::BuildingId warehouse = found.records.at(0).building;
    const auto city_ref = city_of_player(w, player);
    REQUIRE(city_ref.has_value());
    std::int64_t found_wood = 0;
    for (const auto& cg : kontor.cost_goods) {
        found_wood += cg.goods == wood ? cg.amount_88 : 0;
    }
    CHECK(ship_wood_start - aboard_wood(w) == found_wood); // 3072 (12 t) taken from the hold
    std::int64_t unloaded_wood = 0;
    for (std::uint32_t h = 0; h < ship_of(w, ship_id).holds.size(); ++h) {
        const sim::ShipHold hold = ship_of(w, ship_id).holds[h];
        if (hold.goods == 0 || hold.amount_88 == 0) {
            continue;
        }
        const auto tons = static_cast<std::uint32_t>((hold.amount_88 + 255) / 256);
        const sim::ApplyResult r = d.apply(
            sim::Command{sim::PlayerId{player},
                         sim::UnloadCmd{sim::FigureId{ship_id}, warehouse, h, hold.goods, tons}});
        if (r.status == sim::Reject::None) {
            unloaded_wood +=
                hold.goods == wood ? static_cast<std::int64_t>(r.records.at(0).transferred_88) : 0;
        }
        d.advance(100);
        d.checkpoint();
    }
    const auto city_wood = [&](const sim::World& world) {
        return static_cast<std::int64_t>(ct::city(world, *city_ref).stock.stock.at(wood));
    };
    CHECK(unloaded_wood == ship_wood_start - found_wood - aboard_wood(w));
    CHECK(city_wood(w) == unloaded_wood); // the city's wood is exactly the unloaded wood

    // ---- sites, searched on copies: the forester, its road, a real wood receipt --------------
    const sim::Island& home_island = *w.islands()[city_ref->island.v];
    const sim::Building& wh = *w.find_building(warehouse);
    std::vector<ForestPlan> plans;
    for (const Origin& o : legal_origins(home_island, w.types(), forester)) {
        bool mine = true;
        for (std::int32_t dy = 0; dy < 3 && mine; ++dy) {
            for (std::int32_t dx = 0; dx < 3 && mine; ++dx) {
                const sim::Slot idx = home_island.tile_index(o.x + dx, o.y + dy);
                mine = idx != sim::kNoSlot && home_island.tiles.owner(idx) == city_ref->city.v;
            }
        }
        if (!mine || std::max(std::abs(o.x - wh.x), std::abs(o.y - wh.y)) > 16) {
            continue;
        }
        const std::uint32_t trees = trees_in_square(w, home_island, forester, o, baum);
        if (trees >= 6) {
            plans.push_back(ForestPlan{o, {}, trees});
        }
    }
    std::stable_sort(plans.begin(), plans.end(),
                     [](const ForestPlan& a, const ForestPlan& b) { return a.trees > b.trees; });
    REQUIRE_MESSAGE(!plans.empty(), "no forester site with trees near the warehouse");
    const auto try_plan = [&](ForestPlan& p, std::uint32_t max_ticks) -> bool {
        sim::World trial = w; // a copy: the recorded world is never touched by the search
        if (trial.apply(ct::build(player, *city_ref, {ct::at(forester.id, p.site.x, p.site.y)}))
                .status != sim::Reject::None) {
            return false;
        }
        const auto path =
            road_path(trial, *city_ref, road, *trial.find_building(warehouse), kontor, p.site);
        if (!path || path->empty()) {
            return false;
        }
        p.road = *path;
        if (trial.apply(road_drag(player, *city_ref, p.road)).status != sim::Reject::None) {
            return false;
        }
        const std::int64_t w0 = city_wood(trial);
        for (std::uint32_t t = 0; t < max_ticks; t += 50) {
            trial.advance(100);
            if (city_wood(trial) > w0) {
                return true;
            }
        }
        return false;
    };
    std::optional<ForestPlan> chosen, alternative;
    for (std::size_t i = 0; i < plans.size() && i < 8 && (!chosen || !alternative); ++i) {
        ForestPlan p = plans[i];
        if (!chosen) {
            if (try_plan(p, 120000)) {
                chosen = p;
            }
        } else if (std::abs(p.site.x - chosen->site.x) + std::abs(p.site.y - chosen->site.y) > 0) {
            // Another site for the changed-command control: on a copy, the moved forester AND the
            // recorded road drag that follows it both execute (so the replay's divergence comes
            // from an executed placement, not from a refused road).
            sim::World probe = w;
            const bool forester_ok =
                probe.apply(ct::build(player, *city_ref, {ct::at(forester.id, p.site.x, p.site.y)}))
                    .status == sim::Reject::None;
            if (forester_ok && probe.apply(road_drag(player, *city_ref, chosen->road)).status ==
                                   sim::Reject::None) {
                alternative = p;
            }
        }
    }
    REQUIRE_MESSAGE(chosen.has_value(), "no candidate site delivered wood on a copy");
    REQUIRE(alternative.has_value());
    MESSAGE("forester site (" << chosen->site.x << ", " << chosen->site.y << "), " << chosen->trees
                              << " cuttable trees in its square, road of " << chosen->road.size()
                              << " tiles; alternative (" << alternative->site.x << ", "
                              << alternative->site.y << ")");

    // ---- the real run: build the forester and the road through public commands --------------
    sim::TransportStats stats;
    w.set_transport_stats(&stats); // diagnostic only, not state
    const sim::Command build_forester =
        ct::build(player, *city_ref, {ct::at(forester.id, chosen->site.x, chosen->site.y)});
    {
        // The changed commands (the forester at the alternative site, then the same road drag) both
        // execute on a copy of this very state.
        sim::World probe = w;
        probe.set_transport_stats(nullptr);
        CHECK(probe
                  .apply(ct::build(player, *city_ref,
                                   {ct::at(forester.id, alternative->site.x, alternative->site.y)}))
                  .status == sim::Reject::None);
        CHECK(probe.apply(road_drag(player, *city_ref, chosen->road)).status == sim::Reject::None);
    }
    const std::int64_t wood_before_build = city_wood(w);
    const sim::ApplyResult bf = d.apply(build_forester);
    REQUIRE(bf.status == sim::Reject::None);
    const sim::BuildingId forester_id = bf.records.at(0).building;
    const sim::ApplyResult br = d.apply(road_drag(player, *city_ref, chosen->road));
    REQUIRE(br.status == sim::Reject::None);
    d.checkpoint();
    const std::int64_t construction_wood = wood_before_build - city_wood(w);
    CHECK(construction_wood == 0); // forester 150 gold + 2 tools, roads gold only
    const auto farm = [&](const sim::World& world) -> const sim::Farm& {
        return std::get<sim::Farm>(world.find_building(forester_id)->payload);
    };
    CHECK(farm(w).raw == 0); // nothing seeded
    CHECK(farm(w).out == 0);
    // The tree ledger region: the forester's square, without its footprint and the road.
    sim::Building fb;
    fb.type = forester.id;
    fb.x = chosen->site.x;
    fb.y = chosen->site.y;
    const sim::TileRect square = sim::search_area(fb, forester).square();
    std::set<std::pair<int, int>> skip;
    for (std::int32_t dy = 0; dy < 3; ++dy) {
        for (std::int32_t dx = 0; dx < 3; ++dx) {
            skip.insert({chosen->site.x + dx, chosen->site.y + dy});
        }
    }
    for (const Origin& o : chosen->road) {
        skip.insert({o.x, o.y});
    }
    // Every tile change in the square, classified per advance (docs/sim/forestry.md 7): a cut
    // turns a cuttable N into N - 1; the growth sweep turns a growing M into M + 1 (pre-existing
    // young trees mature: measured, not assumed 0); nothing else may change there.
    std::map<std::pair<int, int>, std::uint32_t> last_obj;
    const auto snapshot = [&] {
        const sim::Island& is = *w.islands()[city_ref->island.v];
        for (std::int32_t y = square.y0; y < square.y0 + static_cast<std::int32_t>(square.h); ++y) {
            for (std::int32_t x = square.x0; x < square.x0 + static_cast<std::int32_t>(square.w);
                 ++x) {
                const sim::Slot idx = is.tile_index(x, y);
                if (idx != sim::kNoSlot && !skip.contains({x, y})) {
                    last_obj[{x, y}] = is.tiles.object(idx);
                }
            }
        }
    };
    snapshot();
    std::map<std::uint32_t, std::int64_t> cuts_by_type, growth_by_type;
    std::int64_t unexplained_tile_changes = 0;
    const auto classify = [&] {
        const sim::Island& is = *w.islands()[city_ref->island.v];
        for (auto& [xy, before] : last_obj) {
            const std::uint32_t now = is.tiles.object(is.tile_index(xy.first, xy.second));
            if (now == before) {
                continue;
            }
            if (now + 1 == before && sim::harvest_cuttable(w.types(), before)) {
                ++cuts_by_type[before];
            } else if (now == before + 1 && w.types().find_growing(before) != nullptr) {
                ++growth_by_type[before];
            } else {
                ++unexplained_tile_changes;
            }
            before = now;
        }
    };

    // ---- ordinary advances until new wood arrives, then a little longer ----------------------
    // The raw, production and cart equations of docs/sim/forestry.md 7 at a boundary.
    const auto boundary_balanced = [&] {
        const sim::ForestryStats& bs = stats.forestry;
        std::int64_t active = 0, carts = 0;
        for (const auto& f : w.figures().list.slots) {
            if (!f || f->expiry_stamp) {
                continue;
            }
            if (f->harvest) {
                active += f->cargo_88;
            } else if (f->job == sim::kJobMarketCart && f->cargo_goods == wood) {
                carts += f->cargo_88;
            }
        }
        const auto lit = stats.loaded_from.find(forester_id);
        const std::int64_t ld = lit != stats.loaded_from.end() ? std::int64_t(lit->second) : 0;
        const auto fit = bs.farms.find(forester_id);
        const std::int64_t cons = fit != bs.farms.end() ? std::int64_t(fit->second.consumed_88) : 0;
        const std::int64_t mk = fit != bs.farms.end() ? std::int64_t(fit->second.made_88) : 0;
        const auto off = stats.unload_offered.find(wood);
        const std::int64_t of = off != stats.unload_offered.end() ? std::int64_t(off->second) : 0;
        CHECK(std::int64_t(bs.raw_credited_88) == 256 * std::int64_t(bs.cuts));
        CHECK(std::int64_t(bs.raw_credited_88) - active - std::int64_t(bs.raw_delivered_88) -
                  std::int64_t(bs.raw_lost_88) ==
              0);
        CHECK(std::int64_t(bs.raw_delivered_88) - cons - std::int64_t(farm(w).raw) == 0);
        CHECK(mk - std::int64_t(farm(w).out) - ld == 0);
        CHECK(ld - carts - of == 0);
        CHECK(of - (city_wood(w) - wood_before_build) == 0);
    };
    bool reloaded = false;
    std::uint32_t ticks = 0;
    std::uint32_t settle_after = 0;
    for (; ticks < 200000; ticks += 50) {
        d.advance(100);
        classify();
        if (ticks % 5000 == 0) {
            d.checkpoint();
            boundary_balanced();
        }
        if (!reloaded) {
            // One save/reload while a worker carries wood home: resumed == uninterrupted.
            for (const auto& f : w.figures().list.slots) {
                if (f && f->harvest && f->harvest->step == sim::HarvestStep::ToHome &&
                    f->cargo_88 > 0 && !f->expiry_stamp) {
                    const sim::FigureId carrier = f->id;
                    sim::World copy = w;
                    copy.set_transport_stats(nullptr); // a copy carries the sink: detach it
                    auto back = sim::World::load(w.save(), rs.types);
                    REQUIRE(back.ok());
                    sim::World resumed = std::move(back).value();
                    for (int k = 0; k < 100; ++k) { // 5000 ticks: through the delivery
                        copy.advance(100);
                        resumed.advance(100);
                    }
                    CHECK(resumed.state_hash() == copy.state_hash());
                    CHECK(resumed.save() == copy.save());
                    // The carried wood was delivered in both within the window (the worker ended).
                    for (const sim::World* run : {&copy, &resumed}) {
                        const sim::Figure* g = sim::find_figure(run->figures(), carrier);
                        CHECK((g == nullptr || g->expiry_stamp.has_value()));
                    }
                    reloaded = true;
                    break;
                }
            }
        }
        if (settle_after == 0 && city_wood(w) > wood_before_build) {
            settle_after = ticks + 20000;
        }
        if (settle_after != 0 && ticks >= settle_after) {
            break;
        }
    }
    d.checkpoint();
    CHECK(reloaded);
    REQUIRE_MESSAGE(settle_after != 0, "no new wood reached the warehouse within 200 000 ticks");
    MESSAGE("first new wood at the warehouse after about " << (settle_after - 20000)
                                                           << " ticks; ran " << ticks);

    // ---- the ledger (docs/sim/forestry.md 7): every unexplained term is zero ------------------
    const sim::ForestryStats& fs = stats.forestry;
    const auto lf = stats.loaded_from.find(forester_id);
    const std::int64_t cart_loaded = lf != stats.loaded_from.end() ? std::int64_t(lf->second) : 0;
    const auto at = [](const std::map<sim::GoodsId, std::uint64_t>& m, sim::GoodsId g) {
        const auto i = m.find(g);
        return i != m.end() ? std::int64_t(i->second) : std::int64_t{0};
    };
    const std::int64_t offered = at(stats.unload_offered, wood);
    const std::int64_t cart_lost = at(stats.cart_lost, wood);
    std::int64_t worker_active = 0, cart_active = 0;
    for (const auto& f : w.figures().list.slots) {
        if (!f || f->expiry_stamp) {
            continue;
        }
        if (f->harvest) {
            worker_active += f->cargo_88;
        } else if (f->job == sim::kJobMarketCart && f->cargo_goods == wood) {
            cart_active += f->cargo_88;
        }
    }
    const auto fl = fs.farms.find(forester_id);
    REQUIRE(fl != fs.farms.end());
    const std::int64_t consumed = std::int64_t(fl->second.consumed_88);
    const std::int64_t made = std::int64_t(fl->second.made_88);
    const std::int64_t received = city_wood(w) - wood_before_build;
    MESSAGE("ledger: cuts " << fs.cuts << ", credited " << fs.raw_credited_88 << ", delivered raw "
                            << fs.raw_delivered_88 << ", consumed " << consumed << ", made " << made
                            << ", out " << farm(w).out << ", loaded " << cart_loaded << ", offered "
                            << offered << ", received " << received);
    // ship and city
    CHECK(aboard_wood(w) == ship_wood_start - found_wood - unloaded_wood);
    CHECK(city_wood(w) - (unloaded_wood - construction_wood + received) == 0);
    CHECK(received > 0); // newly produced wood reached the warehouse
    // harvest -> raw -> production -> output -> carts -> receipt
    CHECK(std::int64_t(fs.raw_credited_88) == 256 * std::int64_t(fs.cuts));
    CHECK(std::int64_t(fs.raw_credited_88) - worker_active - std::int64_t(fs.raw_delivered_88) -
              std::int64_t(fs.raw_lost_88) ==
          0);
    CHECK(std::int64_t(fs.raw_delivered_88) - consumed - std::int64_t(farm(w).raw) == 0);
    CHECK(consumed * 14 == made * 32); // every step full: raw arrives in 256s, consumed in 32s
    CHECK(made - std::int64_t(farm(w).out) - cart_loaded == 0);
    CHECK(cart_loaded - cart_active - offered - cart_lost == 0);
    CHECK(offered - received == 0);       // below the capacity: no clip
    CHECK(stats.loaded_from.size() == 1); // the forester is the only source any cart loaded from
    CHECK(fs.cut_mismatches == 0);
    CHECK(fs.raw_lost_88 == 0);
    // the trees: every tile change of the square explained; the cuts seen on the tiles are exactly
    // the successful cuts the workers were credited for
    CHECK(unexplained_tile_changes == 0);
    std::int64_t cut_seen = 0;
    for (const auto& [id, n] : cuts_by_type) {
        CHECK(sim::harvest_cuttable(w.types(), id));
        cut_seen += n;
    }
    std::int64_t grown = 0;
    for (const auto& [id, n] : growth_by_type) {
        grown += n;
    }
    MESSAGE("tiles: " << cut_seen << " cuts over " << cuts_by_type.size() << " tree types, "
                      << grown << " growth steps of pre-existing young objects");
    CHECK(cut_seen == std::int64_t(fs.cuts));
    REQUIRE(w.check_invariants() == "");
    const std::uint64_t final_hash = w.state_hash();
    const std::vector<std::uint8_t> final_save = w.save();

    // ---- replay: in process, through bytes; a changed legal command diverges ----------------
    auto bytes = sim::write_log(rec.log());
    REQUIRE(bytes.ok());
    {
        const std::string e =
            export_log_if_requested(std::getenv("ANNO_FORESTER_LOG"), bytes.value());
        REQUIRE_MESSAGE(e.empty(), e);
    }
    auto read = sim::read_log(bytes.value());
    REQUIRE(read.ok());
    auto fresh = sim::World::load(read.value().initial_save, rs.types, {});
    REQUIRE(fresh.ok());
    auto outcome = sim::replay(fresh.value(), read.value());
    REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
    CHECK(outcome.value().reproduced());
    CHECK(fresh.value().state_hash() == final_hash);
    CHECK(fresh.value().save() == final_save);
    sim::ReplayLog tampered = read.value();
    bool moved = false;
    for (sim::ReplayEntry& e : tampered.entries) {
        for (sim::Command& c : e.commands) {
            auto* b = std::get_if<sim::BuildCmd>(&c.body);
            if (b != nullptr && !moved && b->placements.size() == 1 &&
                b->placements[0].type == forester.id) {
                b->placements[0].x = alternative->site.x; // legal there too (probed above)
                b->placements[0].y = alternative->site.y;
                moved = true;
            }
        }
    }
    REQUIRE(moved);
    {
        const auto tb = sim::write_log(tampered);
        REQUIRE(tb.ok());
        const std::string e =
            export_log_if_requested(std::getenv("ANNO_FORESTER_LOG_TAMPERED"), tb.value());
        REQUIRE_MESSAGE(e.empty(), e);
    }
    auto fresh2 = sim::World::load(tampered.initial_save, rs.types, {});
    REQUIRE(fresh2.ok());
    auto bad = sim::replay(fresh2.value(), tampered);
    CHECK((!bad.ok() || !bad.value().reproduced()));
}

TEST_CASE("bootstrap prefix: unchanged ship sails founds unloads builds then exposes supply and "
          "salt gaps") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    namespace ct = command_test;
    const RealStart rs = real_start(*dir);
    auto loaded = sl::load_ship_start(rs.manifest, *dir, rs.types, {}, rs.rules);
    REQUIRE(loaded.ok());
    auto w = std::move(loaded).value();
    REQUIRE(sl::expedition_baseline_problem(w).empty());
    const sim::World pristine = w;
    auto legs = plan_opening_legs(rs, pristine);
    legs.resize(1);
    auto started = sim::Recorder::start(w);
    REQUIRE(started.ok());
    auto rec = std::move(started).value();
    Driver d{&w, &rec};
    REQUIRE(sail_phase(d, w, legs, false).all_arrived);
    const auto found = d.apply(found_command(legs[0]));
    REQUIRE(found.ok());
    const auto warehouse = found.records.at(0).building;
    const auto ref = city_of_player(w, 0);
    REQUIRE(ref.has_value());
    const auto gold = [&] { return w.players().find(sim::PlayerId{0})->money; };
    CHECK(gold() == 29650);
    for (std::uint32_t i = 0; i < ship_of(w, 1).holds.size(); ++i) {
        const auto h = ship_of(w, 1).holds[i];
        if (h.goods == 0 || h.amount_88 == 0)
            continue;
        const auto r = d.apply(
            sim::Command{sim::PlayerId{0},
                         sim::UnloadCmd{sim::FigureId{1}, warehouse, i, h.goods,
                                        static_cast<std::uint32_t>((h.amount_88 + 255) / 256)}});
        REQUIRE(r.ok());
    }
    const auto stock = [&](sim::GoodsId g) { return ct::city(w, *ref).stock.stock[g]; };
    CHECK(stock(42) == 38 * 256);
    CHECK(stock(41) == 50 * 256);
    CHECK(aboard_of(ship_of(w, 1).holds, 41) == 25 * 256);
    CHECK(stock(26) == 10 * 256);
    const auto eligible = [&](std::uint32_t id) {
        return sim::build_eligibility(w.types(), w.players(), sim::PlayerId{0}, sim::TypeId{id});
    };
    const auto& forester = *w.types().find(sim::TypeId{801});
    const auto& road = *w.types().find(sim::TypeId{kRoadDirt});
    const auto& kontor = *w.types().find(sim::TypeId{1302});
    std::optional<ForestPlan> plan;
    for (const auto& site : legal_origins(*w.islands()[ref->island.v], w.types(), forester)) {
        if (std::max(std::abs(site.x - w.find_building(warehouse)->x),
                     std::abs(site.y - w.find_building(warehouse)->y)) > 16)
            continue;
        sim::World trial = w;
        if (!trial.apply(ct::build(0, *ref, {ct::at(forester.id, site.x, site.y)})).ok())
            continue;
        const auto path =
            road_path(trial, *ref, road, *trial.find_building(warehouse), kontor, site);
        if (path && !path->empty() && trial.apply(road_drag(0, *ref, *path)).ok()) {
            plan = ForestPlan{site, *path, 0};
            break;
        }
    }
    REQUIRE(plan.has_value());
    REQUIRE(eligible(801).allowed());
    REQUIRE(eligible(kRoadDirt).allowed());
    REQUIRE(d.apply(ct::build(0, *ref, {ct::at(forester.id, plan->site.x, plan->site.y)})).ok());
    REQUIRE(d.apply(road_drag(0, *ref, plan->road)).ok());
    CHECK(gold() == 29500 - static_cast<std::int64_t>(5 * plan->road.size()));
    CHECK(stock(42) == 38 * 256);
    CHECK(stock(41) == 48 * 256);
    REQUIRE(eligible(809).allowed());
    sim::BuildingId farm;
    for (const auto& site :
         legal_origins(*w.islands()[ref->island.v], w.types(), *w.types().find(sim::TypeId{809}))) {
        const auto cmd = ct::build(0, *ref, {ct::at(sim::TypeId{809}, site.x, site.y)});
        sim::World trial = w;
        if (!trial.apply(cmd).ok())
            continue;
        const auto r = d.apply(cmd);
        REQUIRE(r.ok());
        farm = r.records.at(0).building;
        break;
    }
    REQUIRE(farm.valid());
    const auto& f = std::get<sim::SmallFarm>(w.find_building(farm)->payload);
    CHECK(f.raw == std::vector<std::uint32_t>{0, 0});
    CHECK(f.out == std::vector<std::uint32_t>{0, 0});
    CHECK(gold() == 29250 - static_cast<std::int64_t>(5 * plan->road.size()));
    CHECK(stock(42) == 34 * 256);
    CHECK(stock(41) == 46 * 256);
    // No stage injection: the content query refuses salt at stage1C. The public command below
    // reaches structural Unsupported before permission checks; wiring is enforced elsewhere.
    CHECK(eligible(1203).status == sim::BuildEligibilityStatus::StageLocked);
    bool salt_unsupported = false;
    const auto& island = *w.islands()[ref->island.v];
    for (std::int32_t y = island.tiles.y0();
         y < island.tiles.y0() + static_cast<std::int32_t>(island.tiles.height()) &&
         !salt_unsupported;
         ++y) {
        for (std::int32_t x = island.tiles.x0();
             x < island.tiles.x0() + static_cast<std::int32_t>(island.tiles.width()); ++x) {
            const auto cmd = ct::build(0, *ref, {ct::at(sim::TypeId{1203}, x, y)});
            sim::World trial = w;
            if (trial.apply(cmd).status != sim::Reject::Unsupported)
                continue;
            const auto before = w.save();
            const auto r = d.apply(cmd);
            CHECK(r.status == sim::Reject::Unsupported);
            CHECK(w.save() == before);
            salt_unsupported = true;
            break;
        }
    }
    REQUIRE(salt_unsupported);
    d.checkpoint();
    REQUIRE(w.check_invariants().empty());
    MESSAGE("BOOTSTRAP_STOP road_tiles="
            << plan->road.size() << " gold=" << gold() << " wood_88=" << stock(42)
            << " tools_city_88=" << stock(41)
            << " tools_ship_88=" << aboard_of(ship_of(w, 1).holds, 41) << " food_88=" << stock(26)
            << " farm_raw=0,0 farm_output=0,0 next_supply=MAP-SUPPLY-1 salt_query=StageLocked "
               "salt_command=Unsupported hash="
            << w.state_hash());
    auto bytes = sim::write_log(rec.log());
    REQUIRE(bytes.ok());
    const auto error = export_log_if_requested(std::getenv("ANNO_BOOTSTRAP_LOG"), bytes.value());
    REQUIRE_MESSAGE(error.empty(), error);
    auto fresh = sim::World::load(rec.log().initial_save, rs.types, {});
    REQUIRE(fresh.ok());
    auto replayed = sim::replay(fresh.value(), rec.log());
    REQUIRE(replayed.ok());
    CHECK(replayed.value().reproduced());
    CHECK(fresh.value().save() == w.save());
    auto reloaded = sim::World::load(w.save(), rs.types, {});
    REQUIRE(reloaded.ok());
    CHECK(reloaded.value().save() == w.save());
}

// MAP-EXPAND-1: bounded planning on disposable copies; no planning state is recorded.
namespace expansion_test {

struct Plan {
    SailLeg source, destination;
    ForestPlan forest;
    Origin receiver;
    std::uint32_t first_stock = 0, first_tons = 0, first_residue = 0;
    std::uint64_t production_ticks = 0;
    std::uint64_t old_wood_pipeline = 0, second_made = 0, second_ticks = 0;
    std::uint32_t second_tons = 0, second_stock = 0;
};

sim::CityRef city_of_warehouse(const sim::World& w, sim::BuildingId warehouse) {
    const auto island = sim::island_of(warehouse);
    REQUIRE(island.valid());
    REQUIRE(island.v < w.islands().size());
    REQUIRE(w.islands()[island.v].has_value());
    const auto& by_id = w.islands()[island.v]->cache.by_id;
    const auto it = by_id.find(warehouse);
    REQUIRE(it != by_id.end());
    return it->second.city;
}

std::uint32_t stock(const sim::World& w, sim::CityRef ref, sim::GoodsId goods) {
    return command_test::city(w, ref).stock.stock.at(goods);
}

sim::RouteResult route(const sim::World& w, sim::FixedPos start, int x, int y) {
    const sim::WorldTileSource source(w, sim::ship_route_terrain_options());
    const sim::CoastIndex index(source, {});
    sim::RouteQuery query;
    query.start_x = start.x >> 8;
    query.start_z = start.y >> 8;
    query.goal_x_q8 = x * 256 + 128;
    query.goal_z_q8 = y * 256 + 128;
    return sim::plan_route(index, query, {}, sim::RouteCostTable::original());
}

// Operator fixture cap, not an engine rule: at most64 water candidates per site.
// Both authoritative founding and cargo checks run on each disposable copy.
std::optional<std::pair<int, int>> dock(const sim::World& w, const SailLeg& leg,
                                        sim::GoodsId tools) {
    const sim::WorldTileSource source(w, sim::ship_route_terrain_options());
    struct Candidate {
        int distance, x, y;
    };
    std::vector<Candidate> candidates;
    for (int dy = -14; dy <= 14; ++dy) {
        for (int dx = -14; dx <= 14; ++dx) {
            const int x = leg.site.click_x + dx, y = leg.site.click_y + dy;
            if (source.tile_is_water(x, y)) {
                candidates.push_back({std::max(std::abs(dx), std::abs(dy)), x, y});
            }
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        return std::tuple{a.distance, a.y, a.x} < std::tuple{b.distance, b.y, b.x};
    });
    std::size_t tried = 0;
    for (const auto& c : candidates) {
        if (c.distance < 4)
            continue;
        if (tried++ == 64)
            break;
        sim::World trial = w;
        trial.set_transport_stats(nullptr);
        sim::WorldTestAccess::relocate(trial, sim::FigureId{leg.ship},
                                       {c.x * 256 + 128, c.y * 256 + 128});
        const auto founded = trial.apply(found_command(leg));
        if (!founded.ok()) {
            MESSAGE("expansion dock " << leg.site.island << " " << c.x << "," << c.y << " founding "
                                      << sim::reject_name(founded.status));
            continue;
        }
        const auto wh = founded.records.at(0).building;
        const auto transfer = trial.apply(
            {sim::PlayerId{leg.player}, sim::UnloadCmd{sim::FigureId{leg.ship}, wh, 2, tools, 2}});
        if (!transfer.ok()) {
            MESSAGE("expansion dock " << leg.site.island << " " << c.x << "," << c.y << " cargo "
                                      << sim::reject_name(transfer.status));
            continue;
        }
        const auto r = route(w, ship_of(w, leg.ship).pos, c.x, c.y);
        MESSAGE("expansion route " << leg.site.island << " " << c.x << "," << c.y << " status "
                                   << static_cast<int>(r.status) << " end " << r.end_x << ","
                                   << r.end_z << " pops " << r.work.coarse_pops << "/"
                                   << r.work.fine_pops);
        if ((r.status == sim::RouteStatus::Planned || r.status == sim::RouteStatus::Arrived) &&
            r.end_x == c.x && r.end_z == c.y)
            return std::pair{c.x, c.y};
    }
    return std::nullopt;
}

// Candidate generation is geometric only. Every yielded candidate still needs
// the authoritative FoundCmd, cargo test and exact route above. At most8 legal
// destination sites across distinct other islands are examined by the caller.
std::vector<Site> destination_sites(const sim::World& w, std::size_t source_island) {
    std::vector<Site> out;
    const auto& type = *w.types().find(sim::TypeId{1302});
    for (std::size_t i = 0; i < w.islands().size() && out.size() < 8; ++i) {
        if (i == source_island || !w.islands()[i] || w.islands()[i]->locked())
            continue;
        const auto& is = *w.islands()[i];
        const auto grid = sim::VertexGrid::of(is);
        if (!grid.ok())
            continue;
        bool found = false;
        for (std::int32_t y = is.tiles.y0();
             y < is.tiles.y0() + static_cast<std::int32_t>(is.tiles.height()) && !found; ++y) {
            for (std::int32_t x = is.tiles.x0();
                 x < is.tiles.x0() + static_cast<std::int32_t>(is.tiles.width()); ++x) {
                const site::Origin o{x, y};
                const auto rotation = site::first_accepted_rotation(0, [&](std::uint8_t turn) {
                    const auto [fw, fh] = sim::footprint(type, turn);
                    return site::footprint_inside(is.tiles, o, fw, fh) &&
                           site::footprint_ownership(is.tiles, w.config().neutral_owner, o, fw,
                                                     fh) == site::Ownership::AllNeutral &&
                           site::footprint_occupancy(is.tiles, w.types(), type, o, fw, fh) ==
                               site::Compat::Compatible &&
                           site::coast_height_check(grid, type, turn, o).result ==
                               site::TerrainResult::Accepted;
                });
                if (rotation) {
                    out.push_back({i, x + 1, y + 1});
                    found = true;
                    break;
                }
            }
        }
    }
    return out;
}

Plan plan(const sim::World& pristine, const RealStart& rs) {
    const auto before = pristine.state_hash();
    const auto wood = *rs.types->goods_by_name("HOLZ");
    const auto tools = *rs.types->goods_by_name("WERKZEUG");
    const auto tree = *rs.types->goods_by_name("BAUM");
    Plan result;
    result.source.player = result.destination.player = 0;
    result.source.ship = result.destination.ship = 1;
    const auto source_site = find_legal_site(pristine, *rs.types->find(sim::TypeId{1302}),
                                             pristine.config().neutral_owner);
    REQUIRE(source_site);
    result.source.site = *source_site;
    const auto source_dock = dock(pristine, result.source, tools);
    REQUIRE_MESSAGE(source_dock, "bounded source docking attempt failed");
    result.source.goal_x = source_dock->first;
    result.source.goal_y = source_dock->second;
    sim::World trial = pristine;
    trial.set_transport_stats(nullptr);
    sim::WorldTestAccess::relocate(
        trial, sim::FigureId{1}, {source_dock->first * 256 + 128, source_dock->second * 256 + 128});
    const auto founded = trial.apply(found_command(result.source));
    REQUIRE(founded.ok());
    const auto warehouse = founded.records.at(0).building;
    const auto ref = city_of_warehouse(trial, warehouse);
    REQUIRE(stock(trial, ref, wood) == 0);
    const auto unload =
        trial.apply({sim::PlayerId{0}, sim::UnloadCmd{sim::FigureId{1}, warehouse, 2, tools, 2}});
    REQUIRE(unload.ok());
    REQUIRE(unload.records.at(0).transferred_88 == 512);
    result.forest.site = {238, 128}; // one reviewed existing-site hint, no fallback
    const auto& forest = *rs.types->find(sim::TypeId{801});
    result.forest.trees =
        trees_in_square(trial, *trial.islands()[ref.island.v], forest, result.forest.site, tree);
    REQUIRE(result.forest.trees > 0);
    const auto built = trial.apply(command_test::build(
        0, ref, {command_test::at(forest.id, result.forest.site.x, result.forest.site.y)}));
    REQUIRE_MESSAGE(built.ok(), sim::reject_name(built.status));
    const auto forest_id = built.records.at(0).building;
    const auto road =
        road_path(trial, ref, *rs.types->find(sim::TypeId{2520}), *trial.find_building(warehouse),
                  *rs.types->find(sim::TypeId{1302}), result.forest.site);
    REQUIRE(road);
    REQUIRE_FALSE(road->empty());
    result.forest.road = *road;
    REQUIRE(trial.apply(road_drag(0, ref, *road)).ok());
    REQUIRE(stock(trial, ref, wood) == 0);
    REQUIRE(stock(trial, ref, tools) == 0);
    const auto& initial_farm = std::get<sim::Farm>(trial.find_building(forest_id)->payload);
    REQUIRE(initial_farm.raw == 0);
    REQUIRE(initial_farm.out == 0);
    REQUIRE(initial_farm.workers_out == 0);
    sim::TransportStats stats;
    trial.set_transport_stats(&stats);
    for (std::uint32_t n = 0; n < 400; ++n) {
        trial.advance(1000);
        REQUIRE_FALSE(trial.faulted());
        const auto amount = stock(trial, ref, wood);
        if (amount >= 256 && amount % 256 != 0) {
            result.first_stock = amount;
            result.production_ticks = trial.clock().tick();
            break;
        }
    }
    trial.set_transport_stats(nullptr);
    REQUIRE_MESSAGE(result.first_stock >= 256, "no fractional receipt in bounded production pilot");
    REQUIRE(stats.forestry.cuts > 0);
    REQUIRE(stats.forestry.raw_delivered_88 > 0);
    REQUIRE(stats.loaded_from.at(forest_id) > 0);
    REQUIRE(stats.unload_offered.at(wood) == result.first_stock);
    result.first_tons = result.first_stock / 256;
    REQUIRE(std::uint64_t{result.first_tons} * 256 <=
            rs.types->find_figure_type(1502)->capacity_88 -
                ship_of(trial, 1).holds.at(0).amount_88);
    const auto load = trial.apply(
        {sim::PlayerId{0}, sim::LoadCmd{sim::FigureId{1}, warehouse, 0, wood, result.first_tons}});
    REQUIRE(load.ok());
    REQUIRE(load.records.at(0).transferred_88 == std::uint64_t{result.first_tons} * 256);
    result.first_residue = stock(trial, ref, wood);
    REQUIRE(result.first_residue == result.first_stock % 256);
    REQUIRE(result.first_residue > 0);
    REQUIRE(result.first_residue < 256);
    // Reserved is inside out, BAUM raw is not wood, ended cargo is history.
    result.old_wood_pipeline =
        result.first_residue +
        std::uint64_t{std::get<sim::Farm>(trial.find_building(forest_id)->payload).out};
    for (const auto& f : trial.figures().list.slots) {
        if (f && !f->expiry_stamp && f->home == warehouse && f->job == sim::kJobMarketCart &&
            f->cargo_goods == wood && f->transport.step != sim::TransportStep::Done) {
            result.old_wood_pipeline += f->cargo_88;
        }
    }
    REQUIRE(result.old_wood_pipeline / 256 < std::numeric_limits<std::uint32_t>::max());
    result.second_tons = static_cast<std::uint32_t>(result.old_wood_pipeline / 256 + 1);
    REQUIRE(std::uint64_t{result.second_tons} * 256 > result.old_wood_pipeline);
    bool selected = false;
    for (const auto& candidate : destination_sites(trial, ref.island.v)) {
        auto leg = result.destination;
        leg.site = candidate;
        const auto target = dock(trial, leg, tools);
        if (!target)
            continue;
        leg.goal_x = target->first;
        leg.goal_y = target->second;
        sim::World destination = trial;
        destination.set_transport_stats(nullptr);
        sim::WorldTestAccess::relocate(destination, sim::FigureId{1},
                                       {leg.goal_x * 256 + 128, leg.goal_y * 256 + 128});
        const auto second = destination.apply(found_command(leg));
        REQUIRE(second.ok());
        const auto second_wh = second.records.at(0).building;
        const auto second_ref = city_of_warehouse(destination, second_wh);
        REQUIRE(second_ref.island != ref.island);
        REQUIRE(command_test::city(destination, second_ref).owner == sim::PlayerId{0});
        REQUIRE(
            destination
                .apply({sim::PlayerId{0}, sim::UnloadCmd{sim::FigureId{1}, second_wh, 2, tools, 1}})
                .ok());
        const auto& receiver = *rs.types->find(sim::TypeId{1310});
        std::optional<Origin> receiver_site;
        for (const auto& o : legal_origins(*destination.islands()[second_ref.island.v],
                                           destination.types(), receiver)) {
            const auto& is = *destination.islands()[second_ref.island.v];
            if (is.tiles.owner(is.tile_index(o.x, o.y)) == second_ref.city.v) {
                receiver_site = o;
                break;
            }
        }
        if (!receiver_site)
            continue;
        const auto build = command_test::build(
            0, second_ref, {command_test::at(receiver.id, receiver_site->x, receiver_site->y)});
        const auto no_goods_hash = destination.state_hash();
        REQUIRE(destination.apply(build).status == sim::Reject::CannotAfford);
        REQUIRE(destination.state_hash() == no_goods_hash);
        const auto freight =
            destination.apply({sim::PlayerId{0}, sim::UnloadCmd{sim::FigureId{1}, second_wh, 0,
                                                                wood, result.first_tons}});
        REQUIRE(freight.ok());
        REQUIRE(freight.records.at(0).transferred_88 == std::uint64_t{result.first_tons} * 256);
        REQUIRE(destination.apply(build).ok());
        const auto back = route(destination, ship_of(destination, 1).pos, result.source.goal_x,
                                result.source.goal_y);
        MESSAGE("expansion return route status "
                << static_cast<int>(back.status) << " end " << back.end_x << "," << back.end_z
                << " pops " << back.work.coarse_pops << "/" << back.work.fine_pops);
        if (back.status != sim::RouteStatus::Planned || back.end_x != result.source.goal_x ||
            back.end_z != result.source.goal_y)
            continue;
        // Planning-only return. The genuine arm must sail and match arrival.
        sim::WorldTestAccess::relocate(
            destination, sim::FigureId{1},
            {result.source.goal_x * 256 + 128, result.source.goal_y * 256 + 128});
        REQUIRE(stock(destination, ref, wood) == result.first_residue);
        const auto room = std::uint64_t{rs.types->find_figure_type(1502)->capacity_88} -
                          ship_of(destination, 1).holds.at(0).amount_88;
        REQUIRE(std::uint64_t{result.second_tons} * 256 <= room);
        sim::TransportStats further;
        destination.set_transport_stats(&further);
        const auto second_start = destination.clock().tick();
        for (std::uint32_t n = 0; n < 400; ++n) {
            destination.advance(1000);
            REQUIRE_FALSE(destination.faulted());
            if (stock(destination, ref, wood) >= std::uint64_t{result.second_tons} * 256) {
                result.second_stock = stock(destination, ref, wood);
                result.second_ticks = destination.clock().tick() - second_start;
                break;
            }
        }
        destination.set_transport_stats(nullptr);
        REQUIRE_MESSAGE(result.second_stock >= std::uint64_t{result.second_tons} * 256,
                        "second export unavailable within bounded production trial");
        result.second_made = further.forestry.farms.at(forest_id).made_88;
        REQUIRE(result.second_made >=
                std::uint64_t{result.second_tons} * 256 - result.old_wood_pipeline);
        const auto second_load =
            destination.apply({sim::PlayerId{0}, sim::LoadCmd{sim::FigureId{1}, warehouse, 0, wood,
                                                              result.second_tons}});
        REQUIRE(second_load.ok());
        REQUIRE(second_load.records.at(0).transferred_88 ==
                std::uint64_t{result.second_tons} * 256);
        const auto repeat = route(destination, ship_of(destination, 1).pos, leg.goal_x, leg.goal_y);
        MESSAGE("expansion repeat route status "
                << static_cast<int>(repeat.status) << " end " << repeat.end_x << "," << repeat.end_z
                << " pops " << repeat.work.coarse_pops << "/" << repeat.work.fine_pops);
        if (repeat.status != sim::RouteStatus::Planned || repeat.end_x != leg.goal_x ||
            repeat.end_z != leg.goal_y)
            continue;
        result.receiver = *receiver_site;
        result.destination = leg;
        selected = true;
        break;
    }
    REQUIRE_MESSAGE(selected, "no distinct destination in bounded eight-site/64-dock search");
    CHECK(pristine.state_hash() == before);
    return result;
}

} // namespace expansion_test

TEST_CASE("expansion E1: unchanged candidate route docks forest and finite budget") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto rs = real_start(*dir);
    rs.rules.city_booking = true;
    rs.rules.save_billed_goods = true;
    REQUIRE_FALSE(rs.rules.house_updates);
    auto made = sl::load_ship_start(rs.manifest, *dir, rs.types, {}, rs.rules);
    REQUIRE_MESSAGE(made.ok(), (made.ok() ? std::string{} : made.error()));
    const sim::World pristine = std::move(made).value();
    REQUIRE(sl::expedition_baseline_problem(pristine).empty());
    REQUIRE(pristine.rules().city_booking);
    REQUIRE_FALSE(pristine.rules().free_build_cheat);
    REQUIRE(pristine.rules().shared_visibility ==
            std::vector<std::vector<std::uint32_t>>{{0, 1, 2, 3, 4}});
    for (std::uint32_t owner = 0; owner < 5; ++owner) {
        const auto& p = *pristine.players().find(sim::PlayerId{owner});
        REQUIRE(p.money == 30000);
        REQUIRE(p.relations.empty());
        REQUIRE_FALSE(p.free_build);
        REQUIRE(p.upkeep_a == 0);
        REQUIRE(p.upkeep_b == 0);
    }
    const auto start = std::chrono::steady_clock::now();
    const auto plan = expansion_test::plan(pristine, rs);
    MESSAGE("EXPANSION_E1 source "
            << plan.source.site.island << " click " << plan.source.site.click_x << ","
            << plan.source.site.click_y << " dock " << plan.source.goal_x << ","
            << plan.source.goal_y << " forest " << plan.forest.site.x << "," << plan.forest.site.y
            << " road " << plan.forest.road.size() << " trees " << plan.forest.trees
            << " productionticks " << plan.production_ticks << " receipt " << plan.first_stock
            << " loadtons " << plan.first_tons << " residue " << plan.first_residue
            << " oldpipeline " << plan.old_wood_pipeline << " secondtons " << plan.second_tons
            << " secondmade " << plan.second_made << " secondstock " << plan.second_stock
            << " secondticks " << plan.second_ticks << " destination "
            << plan.destination.site.island << " click " << plan.destination.site.click_x << ","
            << plan.destination.site.click_y << " dock " << plan.destination.goal_x << ","
            << plan.destination.goal_y << " receiver " << plan.receiver.x << "," << plan.receiver.y
            << " planning_ms "
            << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                   .count());
}

namespace expansion_test {

Plan selected_recipe() {
    // Fixture coordinates selected and independently reviewed by E1; every
    // command is revalidated on the actual sailed world, never a relocated one.
    Plan p;
    p.source = {0, 1, {0, 255, 138}, 257, 134};
    p.destination = {0, 1, {1, 456, 353}, 452, 349};
    p.forest.site = {238, 128};
    p.receiver = {450, 355};
    return p;
}

std::map<std::uint32_t, std::int64_t> balances(const sim::World& w) {
    std::map<std::uint32_t, std::int64_t> result;
    for (std::size_t i = 0; i < w.players().slots.size(); ++i)
        if (w.players().slots[i])
            result.emplace(static_cast<std::uint32_t>(i), w.players().slots[i]->money);
    return result;
}

void publish(const char*, const std::vector<std::uint8_t>&);
auto city_view(const sim::City& c) {
    return std::tuple{c.stock.stock,          c.billed,
                      c.accounts.stamp,       c.accounts.sales,
                      c.accounts.trade_sales, c.accounts.trade_purchases,
                      c.accounts.civil_cost,  c.accounts.military_cost};
}
// Exact small scopes at each entry; complete tile/timer/world bytes remain checked at every
// applicable recording checkpoint and final. This avoids hashing every island tile every visit.
std::vector<std::uint8_t> actor_city_bytes(const sim::World& world) {
    sim::WriteArchive out;
    sim::fields(out, world.clock());
    sim::fields(out, world.players());
    sim::fields(out, world.figures());
    out.u64(world.islands().size());
    const sim::CityStream context{world.rules().save_billed_goods, world.config().warehouse_goods};
    for (const auto& island : world.islands()) {
        out.boolean(island.has_value());
        if (!island)
            continue;
        out.u32(island->index.v);
        out.u64(island->cities.size());
        for (const auto& city : island->cities) {
            out.boolean(city.has_value());
            if (city)
                sim::fields(out, *city, context);
        }
    }
    return out.take();
}
struct NativeFollower {
    std::string label;
    sim::World world;
    std::array<std::uint64_t, sim::kDrawSiteCount> initial_draws{}, draws{};
    std::uint64_t advances = 0, commands = 0;
};
struct Run {
    sim::World& world;
    sim::Recorder& recorder;
    std::map<std::uint32_t, std::uint64_t> construction, income, upkeep;
    std::map<sim::CityRef, std::uint64_t> city_upkeep;
    std::vector<sim::SimEvent> events;
    std::uint64_t advances = 0;
    std::vector<NativeFollower> natives{};
    std::array<std::uint64_t, sim::kDrawSiteCount> draws{};
    std::function<void()> source_check{};

    sim::ApplyResult apply(const sim::Command& command, std::uint64_t expected_gold = 0) {
        auto money = balances(world);
        using View = decltype(city_view(std::declval<const sim::City&>()));
        std::map<sim::CityRef, View> cities;
        for (const auto& island : world.islands())
            if (island)
                for (const auto& city : island->cities)
                    if (city)
                        cities.emplace(sim::CityRef{island->index, city->number}, city_view(*city));
        std::optional<sim::CityRef> target;
        if (const auto* b = std::get_if<sim::BuildCmd>(&command.body))
            target = b->city;
        if (const auto* l = std::get_if<sim::LoadCmd>(&command.body))
            target = city_of_warehouse(world, l->warehouse);
        if (const auto* u = std::get_if<sim::UnloadCmd>(&command.body))
            target = city_of_warehouse(world, u->warehouse);
        const auto cursor = world.rng().cursor();
        auto answer = recorder.submit(command);
        if (answer.ok()) {
            money.at(command.issuer.v) -= static_cast<std::int64_t>(expected_gold);
            construction[command.issuer.v] += expected_gold;
            if (std::holds_alternative<sim::FoundCmd>(command.body))
                CHECK(world.rng().cursor() == (cursor + 1) % world.rng().size());
        }
        CHECK(balances(world) == money);
        for (const auto& [ref, view] : cities)
            if (!answer.ok() || !target || *target != ref)
                CHECK(city_view(command_test::city(world, ref)) == view);
        events = world.drain_events();
        REQUIRE_FALSE(world.faulted());
        const auto expected_live = actor_city_bytes(world);
        for (auto& native : natives) {
            const auto resumed = native.world.apply(command);
            REQUIRE(resumed.status == answer.status);
            REQUIRE(resumed.records.size() == answer.records.size());
            for (std::size_t k = 0; k < answer.records.size(); ++k) {
                CHECK(resumed.records[k].status == answer.records[k].status);
                CHECK(resumed.records[k].building == answer.records[k].building);
                CHECK(resumed.records[k].transferred_88 == answer.records[k].transferred_88);
                CHECK(resumed.records[k].tile_object == answer.records[k].tile_object);
            }
            CHECK(native.world.drain_events() == events);
            CHECK(native.world.rng().cursor() == world.rng().cursor());
            CHECK(actor_city_bytes(native.world) == expected_live);
            ++native.commands;
        }
        return answer;
    }
    void advance(std::uint32_t ms = 1000) {
        // This recipe uses1000ms:500ticks exceeds the default50tick city-ring sweep, so every
        // present city is visited. This is a test-driver partition, not an engine restriction.
        REQUIRE(ms == 1000);
        REQUIRE(world.config().city_ring_div == 50);
        std::map<sim::CityRef, std::uint16_t> old_stamps;
        for (const auto& island : world.islands())
            if (island)
                for (const auto& city : island->cities)
                    if (city)
                        old_stamps.emplace(sim::CityRef{island->index, city->number},
                                           city->accounts.stamp);
        auto money = balances(world);
        for (const auto& player : world.players().slots)
            if (player) {
                REQUIRE(player->upkeep_a == 0);
                REQUIRE(player->upkeep_b == 0);
                REQUIRE(player->upkeep_rem == 0);
            }
        recorder.advance(ms);
        events = world.drain_events();
        REQUIRE_FALSE(world.faulted());
        std::set<sim::CityRef> due, booked;
        for (const auto& [ref, stamp] : old_stamps)
            if (static_cast<std::uint16_t>(world.clock().tick() - stamp) >=
                world.config().economy.period)
                due.insert(ref);
        for (const auto& event : events)
            if (event.booking) {
                const auto& b = *event.booking;
                REQUIRE(b.fault == sim::AccountingFault::None);
                REQUIRE(booked.insert(b.city).second);
                const auto& city = command_test::city(world, b.city);
                REQUIRE(city.owner == b.owner);
                REQUIRE(b.money_before == money.at(b.owner.v));
                REQUIRE(b.income == 0);
                REQUIRE(b.upkeep == (city.accounts.civil_cost + city.accounts.military_cost) / 15);
                money.at(b.owner.v) += static_cast<std::int64_t>(b.income);
                money.at(b.owner.v) -= static_cast<std::int64_t>(b.upkeep);
                REQUIRE(b.money_after == money.at(b.owner.v));
                income[b.owner.v] += b.income;
                upkeep[b.owner.v] += b.upkeep;
                city_upkeep[b.city] += b.upkeep;
                CHECK(city.accounts.stamp == static_cast<std::uint16_t>(world.clock().tick()));
            }
        REQUIRE(booked == due); // independently catches omitted/double/merged same-owner charges
        CHECK(balances(world) == money);
        for (const auto& d : world.last_output().draws)
            draws[static_cast<std::size_t>(d.site)] += d.count;
        const auto expected_live = actor_city_bytes(world);
        for (auto& native : natives) {
            native.world.advance(ms);
            REQUIRE_FALSE(native.world.faulted());
            CHECK(native.world.drain_events() == events);
            CHECK(native.world.last_output().events == world.last_output().events);
            const auto& a = native.world.last_output().draws;
            const auto& b = world.last_output().draws;
            REQUIRE(a.size() == b.size());
            for (std::size_t k = 0; k < a.size(); ++k) {
                CHECK(a[k].site == b[k].site);
                CHECK(a[k].count == b[k].count);
                native.draws[static_cast<std::size_t>(a[k].site)] += a[k].count;
            }
            CHECK(native.world.rng().cursor() == world.rng().cursor());
            CHECK(actor_city_bytes(native.world) == expected_live);
            ++native.advances;
        }
        ++advances;
        if (source_check)
            source_check();
    }
    void checkpoint() {
        recorder.checkpoint();
        const auto bytes = world.save();
        for (const auto& native : natives) {
            CHECK(native.world.save() == bytes);
            for (std::size_t k = 0; k < draws.size(); ++k)
                CHECK(native.draws[k] == draws[k] - native.initial_draws[k]);
        }
    }
    void native_cut(const std::string& label, std::shared_ptr<const sim::TypeRegistry> types) {
        REQUIRE(natives.size() <
                5); // exactly five packet cuts; test-only finite proof, no engine cap
        if (natives.empty())
            natives.reserve(5); // keep resumed worlds at stable addresses
        const auto bytes = world.save();
        publish(("cut-" + label + ".save").c_str(), bytes);
        auto loaded = sim::World::load(bytes, std::move(types));
        REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string{} : loaded.error()));
        loaded.value().set_transport_stats(nullptr);
        (void)loaded.value().drain_events();
        CHECK(loaded.value().save() == bytes);
        natives.push_back(NativeFollower{label, std::move(loaded).value(), draws});
        checkpoint();
        MESSAGE("EXPANSION_NATIVE_CUT " << label << " tick " << world.clock().tick() << " entries "
                                        << recorder.log().entries.size());
    }
};

void sail(Run& run, const SailLeg& leg, std::shared_ptr<const sim::TypeRegistry> mid_types = {}) {
    REQUIRE(run.apply(sail_command(leg)).ok());
    const auto order = ship_of(run.world, leg.ship).nav->order->order_id;
    bool arrived = false;
    const auto begin = run.world.clock().tick();
    for (std::uint32_t n = 0; n < 2000 && !arrived; ++n) {
        const auto previous = ship_of(run.world, leg.ship).pos;
        const auto stamp = ship_of(run.world, leg.ship).nav->last;
        run.advance();
        const auto& ship = ship_of(run.world, leg.ship);
        const auto elapsed = static_cast<std::uint16_t>(ship.nav->last - stamp);
        const auto speed = run.world.types().find_figure_type(ship.kind)->nav->speed_q32;
        const std::uint64_t numerator = std::uint64_t{elapsed} * speed * 256;
        const std::uint64_t denominator = 5ULL * (1ULL << 32);
        const auto maximum =
            static_cast<std::int64_t>((numerator + denominator - 1) / denominator + 1);
        CHECK(std::abs(std::int64_t{ship.pos.x} - previous.x) <= maximum);
        CHECK(std::abs(std::int64_t{ship.pos.y} - previous.y) <= maximum);
        for (const auto& event : run.events) {
            if (event.figure != sim::FigureId{leg.ship} || event.order_id != order)
                continue;
            REQUIRE(event.kind != sim::SimEventKind::ShipRouteFailed);
            if (event.kind == sim::SimEventKind::ShipArrived)
                arrived = true;
        }
        REQUIRE(ship_of(run.world, 2).pos == ship.pos);
        REQUIRE(ship_of(run.world, 3).pos == ship_of(run.world, 2).pos);
        if (mid_types && n == 19) {
            REQUIRE_FALSE(arrived);
            run.native_cut("02-mid-voyage", mid_types);
        }
    }
    REQUIRE_MESSAGE(arrived, "reviewed per-leg work cap exhausted before actual arrival");
    CHECK((ship_of(run.world, leg.ship).pos.x >> 8) == leg.goal_x);
    CHECK((ship_of(run.world, leg.ship).pos.y >> 8) == leg.goal_y);
    run.checkpoint();
    MESSAGE("expansion sailed order " << order << " dock " << leg.goal_x << "," << leg.goal_y
                                      << " elapsed_ticks " << run.world.clock().tick() - begin);
}

std::uint64_t cart_wood(const sim::World& world, sim::BuildingId warehouse, sim::GoodsId wood) {
    std::uint64_t result = 0;
    for (const auto& f : world.figures().list.slots)
        if (f && !f->expiry_stamp && f->home == warehouse && f->job == sim::kJobMarketCart &&
            f->cargo_goods == wood && f->transport.step != sim::TransportStep::Done)
            result += f->cargo_88;
    return result;
}

void source_ledger(const sim::World& w, sim::CityRef ref, sim::BuildingId warehouse,
                   sim::BuildingId forest, sim::GoodsId wood, const sim::TransportStats& stats,
                   std::uint64_t exported) {
    const auto& f = std::get<sim::Farm>(w.find_building(forest)->payload);
    const auto value = [](const auto& map, const auto& key) -> std::uint64_t {
        const auto it = map.find(key);
        return it == map.end() ? 0 : it->second;
    };
    std::uint64_t active_raw = 0;
    for (const auto& figure : w.figures().list.slots)
        if (figure && !figure->expiry_stamp && figure->home == forest && figure->harvest)
            active_raw += figure->cargo_88;
    const auto& h = stats.forestry;
    const auto farm = h.farms.find(forest);
    const auto consumed = farm == h.farms.end() ? 0 : farm->second.consumed_88;
    const auto made = farm == h.farms.end() ? 0 : farm->second.made_88;
    const auto loaded = value(stats.loaded_from, forest);
    const auto offered = value(stats.unload_offered, wood);
    REQUIRE(h.raw_credited_88 == h.cuts * 256);
    REQUIRE(h.raw_credited_88 == active_raw + h.raw_delivered_88 + h.raw_lost_88);
    REQUIRE(h.raw_delivered_88 == consumed + f.raw);
    REQUIRE(made == f.out + loaded); // reservation already belongs to out
    REQUIRE(loaded == cart_wood(w, warehouse, wood) + offered + value(stats.cart_lost, wood));
    REQUIRE(offered == stock(w, ref, wood) + exported);
    REQUIRE(h.raw_lost_88 == 0);
    REQUIRE(value(stats.cart_lost, wood) == 0);
    REQUIRE(h.cut_mismatches == 0);
}

void publish(const char* name, const std::vector<std::uint8_t>& bytes) {
    const char* dir = std::getenv("ANNO_EXPAND_OUT_DIR");
    if (dir && *dir) {
        const auto target = fs::path(dir) / name;
        const auto error = export_log_if_requested(target.string().c_str(), bytes);
        REQUIRE_MESSAGE(error.empty(), error);
    }
}

} // namespace expansion_test

TEST_CASE("expansion voyage: genuine founding production and manual resupply") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto rs = real_start(*dir);
    rs.rules.city_booking = true;
    rs.rules.save_billed_goods = true;
    auto made = sl::load_ship_start(rs.manifest, *dir, rs.types, {}, rs.rules);
    REQUIRE_MESSAGE(made.ok(), (made.ok() ? std::string{} : made.error()));
    sim::World w = std::move(made).value();
    REQUIRE(sl::expedition_baseline_problem(w).empty());
    REQUIRE_FALSE(w.rules().house_updates);
    const auto recipe = expansion_test::selected_recipe();
    const auto wood = *rs.types->goods_by_name("HOLZ");
    const auto tools = *rs.types->goods_by_name("WERKZEUG");
    const auto initial_holds = ship_of(w, 1).holds;
    const auto initial_money = expansion_test::balances(w);
    auto recording = sim::Recorder::start(w);
    REQUIRE(recording.ok());
    auto rec = std::move(recording).value();
    expansion_test::Run run{w, rec, {}, {}, {}, {}, {}, 0};
    expansion_test::sail(run, recipe.source);
    REQUIRE(ship_of(w, 1).holds == initial_holds);
    const auto founded = run.apply(found_command(recipe.source), 350);
    REQUIRE(founded.ok());
    const auto warehouse = founded.records.at(0).building;
    const auto ref = expansion_test::city_of_warehouse(w, warehouse);
    REQUIRE(ref.island.v == recipe.source.site.island);
    REQUIRE(command_test::city(w, ref).owner == sim::PlayerId{0});
    REQUIRE(expansion_test::stock(w, ref, wood) == 0);
    REQUIRE(aboard_of(ship_of(w, 1).holds, wood) == 12800 - 3072);
    REQUIRE(aboard_of(ship_of(w, 1).holds, tools) == 20480 - 1280);
    const auto unload =
        run.apply({sim::PlayerId{0}, sim::UnloadCmd{sim::FigureId{1}, warehouse, 2, tools, 2}});
    REQUIRE(unload.ok());
    REQUIRE(unload.records.at(0).transferred_88 == 512);
    const auto built =
        run.apply(command_test::build(0, ref,
                                      {command_test::at(sim::TypeId{801}, recipe.forest.site.x,
                                                        recipe.forest.site.y)}),
                  150);
    REQUIRE_MESSAGE(built.ok(), sim::reject_name(built.status));
    const auto forest = built.records.at(0).building;
    const auto road =
        road_path(w, ref, *rs.types->find(sim::TypeId{2520}), *w.find_building(warehouse),
                  *rs.types->find(sim::TypeId{1302}), recipe.forest.site);
    REQUIRE(road);
    REQUIRE_FALSE(road->empty());
    const auto roads = run.apply(road_drag(0, ref, *road), 5 * road->size());
    REQUIRE(roads.ok());
    REQUIRE(roads.records.size() == road->size());
    for (const auto& record : roads.records) {
        REQUIRE(record.status == sim::Reject::None);
        REQUIRE(record.tile_object);
        REQUIRE_FALSE(record.building.valid());
    }
    REQUIRE(expansion_test::stock(w, ref, wood) == 0);
    REQUIRE(expansion_test::stock(w, ref, tools) == 0);
    const auto& farm = std::get<sim::Farm>(w.find_building(forest)->payload);
    REQUIRE(farm.raw == 0);
    REQUIRE(farm.out == 0);
    REQUIRE(farm.workers_out == 0);
    const auto preproduction = w.save();
    expansion_test::publish("pre-production.save", preproduction);
    sim::TransportStats stats;
    w.set_transport_stats(&stats);
    const auto production_start = w.clock().tick();
    const auto square =
        sim::search_area(*w.find_building(forest), *rs.types->find(sim::TypeId{801})).square();
    std::map<std::pair<std::int32_t, std::int32_t>, std::uint32_t> objects;
    const auto& source_island = *w.islands()[ref.island.v];
    for (std::int32_t y = square.y0; y < square.y0 + static_cast<std::int32_t>(square.h); ++y)
        for (std::int32_t x = square.x0; x < square.x0 + static_cast<std::int32_t>(square.w); ++x) {
            const auto tile = source_island.tile_index(x, y);
            if (tile != sim::kNoSlot)
                objects[{x, y}] = source_island.tiles.object(tile);
        }
    std::uint64_t observed_cuts = 0, observed_growth = 0;
    std::uint32_t calls = 0;
    for (; calls < 400; ++calls) {
        run.advance();
        for (auto& [xy, before] : objects) {
            const auto& island = *w.islands()[ref.island.v];
            const auto now = island.tiles.object(island.tile_index(xy.first, xy.second));
            if (now == before)
                continue;
            if (now + 1 == before && sim::harvest_cuttable(w.types(), before))
                ++observed_cuts;
            else if (now == before + 1 && w.types().find_growing(before) != nullptr)
                ++observed_growth;
            else
                FAIL("unexplained source tree/object transition");
            before = now;
        }
        REQUIRE(observed_cuts == stats.forestry.cuts);
        expansion_test::source_ledger(w, ref, warehouse, forest, wood, stats, 0);
        const auto available = expansion_test::stock(w, ref, wood);
        if (available >= 256 && available % 256 != 0) {
            ++calls;
            break;
        }
    }
    const auto available = expansion_test::stock(w, ref, wood);
    REQUIRE(available >= 256);
    REQUIRE(available % 256 != 0);
    REQUIRE(stats.forestry.cuts > 0);
    const auto tons = available / 256;
    const auto aboard_before = aboard_of(ship_of(w, 1).holds, wood);
    auto first_load_before = w;
    first_load_before.set_transport_stats(nullptr);
    const sim::LoadCmd first_request{sim::FigureId{1}, warehouse, 0, wood, tons};
    const auto load = run.apply({sim::PlayerId{0}, first_request});
    REQUIRE(load.ok());
    const std::uint64_t transferred = std::uint64_t{tons} * 256;
    REQUIRE(cargo_oracle::load_effect(first_load_before, w, sim::PlayerId{0}, first_request, ref, 0,
                                      transferred) == cargo_oracle::Difference::None);
    REQUIRE(load.records.at(0).transferred_88 == transferred);
    REQUIRE(aboard_of(ship_of(w, 1).holds, wood) ==
            aboard_before + static_cast<std::int64_t>(transferred));
    REQUIRE(expansion_test::stock(w, ref, wood) == available % 256);
    auto expected_holds = initial_holds;
    expected_holds.at(0).amount_88 = 12800 - 3072 + transferred;
    expected_holds.at(2).amount_88 = 12800 - 1280 - 512;
    REQUIRE(ship_of(w, 1).holds == expected_holds); // full ordered holds, food and duplicate tools
    REQUIRE(aboard_of(ship_of(w, 1).holds, wood) >= 3072);        // second founding reserve
    REQUIRE(aboard_of(ship_of(w, 1).holds, tools) >= 1280 + 256); // founding plus receiver
    expansion_test::source_ledger(w, ref, warehouse, forest, wood, stats, transferred);
    run.checkpoint();

    // Unfaulted native fork before any production; public switch is the only
    // changed control input. No tree, worker, stock, hold or bill is edited.
    auto loaded = sim::World::load(preproduction, rs.types);
    REQUIRE(loaded.ok());
    auto control = std::move(loaded).value();
    REQUIRE(control.save() == preproduction);
    const auto control_money_before = expansion_test::balances(control);
    REQUIRE(command_test::city(control, ref).accounts.civil_cost == 27);
    REQUIRE(command_test::city(control, ref).accounts.military_cost == 0);
    const auto off = control.apply(
        {sim::PlayerId{0},
         sim::SetProductionLineCmd{{recipe.forest.site.x, recipe.forest.site.y}, wood, false}});
    REQUIRE(off.ok());
    REQUIRE(expansion_test::balances(control) == control_money_before);
    REQUIRE(command_test::city(control, ref).accounts.civil_cost == 19); // 15 + off801(4)
    REQUIRE(command_test::city(control, ref).accounts.military_cost == 0);
    std::uint64_t control_upkeep = 0;
    (void)control.drain_events();
    for (std::uint32_t n = 0; n < calls; ++n) {
        auto money = expansion_test::balances(control);
        for (const auto& player : control.players().slots)
            if (player) {
                REQUIRE(player->upkeep_a == 0);
                REQUIRE(player->upkeep_b == 0);
                REQUIRE(player->upkeep_rem == 0);
            }
        control.advance(1000);
        REQUIRE_FALSE(control.faulted());
        for (const auto& event : control.drain_events())
            if (event.booking) {
                const auto& b = *event.booking;
                REQUIRE(b.fault == sim::AccountingFault::None);
                REQUIRE(b.city == ref);
                REQUIRE(b.owner == sim::PlayerId{0});
                REQUIRE(b.income == 0);
                REQUIRE(b.upkeep == 1); // floor19/15, main floor27/15 also1
                REQUIRE(b.money_before == money.at(0));
                money.at(0) -= 1;
                REQUIRE(b.money_after == money.at(0));
                control_upkeep += b.upkeep;
            }
        REQUIRE(expansion_test::balances(control) == money);
    }
    REQUIRE(control_upkeep > 0);
    REQUIRE(control_upkeep == run.city_upkeep.at(ref));
    REQUIRE(expansion_test::balances(control) == expansion_test::balances(w));
    REQUIRE(expansion_test::stock(control, ref, wood) == 0);
    const auto& stopped = std::get<sim::Farm>(control.find_building(forest)->payload);
    REQUIRE(stopped.raw == 0);
    REQUIRE(stopped.out == 0);
    REQUIRE(stopped.workers_out == 0);
    const auto unchanged = control.state_hash();
    const auto no_export =
        control.apply({sim::PlayerId{0}, sim::LoadCmd{sim::FigureId{1}, warehouse, 0, wood, tons}});
    REQUIRE(no_export.status == sim::Reject::StockShort);
    REQUIRE(control.state_hash() == unchanged);
    REQUIRE(control.check_invariants().empty());
    REQUIRE(w.check_invariants().empty());
    for (const auto& [owner, cash] : initial_money)
        CHECK(expansion_test::balances(w).at(owner) ==
              cash - static_cast<std::int64_t>(run.construction[owner] + run.upkeep[owner]) +
                  static_cast<std::int64_t>(run.income[owner]));
    const auto bytes = sim::write_log(rec.log());
    REQUIRE(bytes.ok());
    expansion_test::publish("genuine.log", bytes.value());
    expansion_test::publish("source-export.save", w.save());
    MESSAGE("EXPANSION_E2 tick "
            << w.clock().tick() << " production_ticks " << w.clock().tick() - production_start
            << " roads " << road->size() << " cuts " << stats.forestry.cuts << " receipt "
            << available << " export " << transferred << " residue "
            << expansion_test::stock(w, ref, wood) << " gold " << gold_of(w, 0) << " spent "
            << run.construction[0] << " upkeep " << run.upkeep[0] << " no_production "
            << sim::reject_name(no_export.status) << " raw_credited "
            << stats.forestry.raw_credited_88 << " raw_delivered "
            << stats.forestry.raw_delivered_88 << " consumed "
            << stats.forestry.farms.at(forest).consumed_88 << " made "
            << stats.forestry.farms.at(forest).made_88 << " farm_raw "
            << std::get<sim::Farm>(w.find_building(forest)->payload).raw << " farm_out "
            << std::get<sim::Farm>(w.find_building(forest)->payload).out << " cart_active "
            << expansion_test::cart_wood(w, warehouse, wood) << " observed_cuts " << observed_cuts
            << " growth " << observed_growth);
    // First-export old-made wood excludes raw BAUM and reserved (already inside Farm.out).
    const auto first_made = stats.forestry.farms.at(forest).made_88;
    const auto first_pipeline = std::uint64_t{expansion_test::stock(w, ref, wood)} +
                                std::get<sim::Farm>(w.find_building(forest)->payload).out +
                                expansion_test::cart_wood(w, warehouse, wood);
    std::uint64_t exported_total = transferred;
    run.source_check = [&] {
        for (auto& [xy, before] : objects) {
            const auto& island = *w.islands()[ref.island.v];
            const auto now = island.tiles.object(island.tile_index(xy.first, xy.second));
            if (now == before)
                continue;
            if (now + 1 == before && sim::harvest_cuttable(w.types(), before))
                ++observed_cuts;
            else if (now == before + 1 && w.types().find_growing(before) != nullptr)
                ++observed_growth;
            else
                FAIL("unexplained source-tree object transition during voyage");
            before = now;
        }
        REQUIRE(stats.forestry.cuts == observed_cuts);
        expansion_test::source_ledger(w, ref, warehouse, forest, wood, stats, exported_total);
    };
    run.native_cut("01-loaded-outbound", rs.types);
    expansion_test::sail(run, recipe.destination, rs.types);
    auto held = ship_of(w, 1).holds;
    const auto second_found = run.apply(found_command(recipe.destination), 350);
    REQUIRE(second_found.ok());
    const auto destination_wh = second_found.records.at(0).building;
    const auto destination_ref = expansion_test::city_of_warehouse(w, destination_wh);
    REQUIRE(destination_ref.island != ref.island);
    REQUIRE(destination_wh != warehouse);
    CHECK(command_test::city(w, destination_ref).owner == sim::PlayerId{0});
    CHECK(command_test::city(w, ref).owner == sim::PlayerId{0});
    held[0].amount_88 -= 12 * 256;
    held[2].amount_88 -= 5 * 256;
    REQUIRE(ship_of(w, 1).holds == held);
    const auto* destination_building = w.find_building(destination_wh);
    REQUIRE(destination_building != nullptr);
    CHECK(destination_building->type == sim::TypeId{1302});
    CHECK(destination_building->x == 455);
    CHECK(destination_building->y == 352);
    CHECK(destination_building->rotation == 2);  // independent original-template bottom shore edge
    CHECK(destination_building->byte0f == 0x30); // original accepted coast max&f0, ship-founding§4
    const auto& destination_island = *w.islands()[destination_ref.island.v];
    for (std::int32_t y = 352; y < 356; ++y)
        for (std::int32_t x = 455; x < 459; ++x) {
            const auto tile = destination_island.tile_index(x, y);
            REQUIRE(tile != sim::kNoSlot);
            CHECK(destination_island.tiles.owner(tile) == destination_ref.city.v);
            CHECK(destination_island.tiles.object(tile) == 1302);
            CHECK(destination_island.tiles.item_height(tile) == 0x30);
            const auto* at = w.building_at(destination_ref, x, y);
            REQUIRE(at != nullptr);
            CHECK(at->id == destination_wh);
        }
    REQUIRE(command_test::city(w, destination_ref).accounts.civil_cost == 15);
    REQUIRE(expansion_test::stock(w, destination_ref, wood) == 0);
    run.native_cut("03-immediately-second-founded", rs.types);
    const auto tool_freight = run.apply(
        {sim::PlayerId{0}, sim::UnloadCmd{sim::FigureId{1}, destination_wh, 2, tools, 1}});
    REQUIRE(tool_freight.ok());
    REQUIRE(tool_freight.records.at(0).transferred_88 == 256);
    held[2].amount_88 -= 256;
    REQUIRE(ship_of(w, 1).holds == held);
    REQUIRE(expansion_test::stock(w, destination_ref, tools) == 256);
    const auto receiver = command_test::build(
        0, destination_ref,
        {command_test::at(sim::TypeId{1310}, recipe.receiver.x, recipe.receiver.y)});
    const auto before_wood = w.save();
    REQUIRE(run.apply(receiver).status == sim::Reject::CannotAfford);
    REQUIRE(w.save() == before_wood);
    const auto freight = run.apply(
        {sim::PlayerId{0}, sim::UnloadCmd{sim::FigureId{1}, destination_wh, 0, wood, tons}});
    REQUIRE(freight.ok());
    REQUIRE(freight.records.at(0).transferred_88 == transferred);
    held[0].amount_88 -= transferred;
    REQUIRE(ship_of(w, 1).holds == held);
    REQUIRE(expansion_test::stock(w, destination_ref, wood) == transferred);
    run.native_cut("04-after-complete-first-unload", rs.types);
    const auto level = window_level(destination_island, *w.types().find(sim::TypeId{1310}),
                                    recipe.receiver.x, recipe.receiver.y);
    REQUIRE(level.has_value());
    const auto received_building = run.apply(receiver, 50);
    REQUIRE(received_building.ok());
    const auto* placed = w.find_building(received_building.records.at(0).building);
    REQUIRE(placed != nullptr);
    CHECK(placed->byte0f == *level);
    CHECK(destination_island.tiles.item_height(
              destination_island.tile_index(recipe.receiver.x, recipe.receiver.y)) == *level);
    CHECK(expansion_test::stock(w, destination_ref, wood) == transferred - 256);
    CHECK(expansion_test::stock(w, destination_ref, tools) == 0);
    CHECK(command_test::city(w, destination_ref).accounts.civil_cost == 20);
    auto end_holds = initial_holds;
    end_holds[0].amount_88 -= 24 * 256;
    end_holds[2].amount_88 -= 13 * 256;
    CHECK(ship_of(w, 1).holds == end_holds);
    expansion_test::source_ledger(w, ref, warehouse, forest, wood, stats, transferred);
    for (const auto& actor : w.figures().list.slots)
        if (actor && actor->home.valid()) {
            CHECK(actor->owner == sim::PlayerId{0});
            CHECK((actor->home == warehouse ||
                   actor->home == forest)); // source carrier/harvester homes
        }
    for (const auto& [owner, cash] : initial_money)
        CHECK(expansion_test::balances(w).at(owner) ==
              cash - static_cast<std::int64_t>(run.construction[owner] + run.upkeep[owner]) +
                  static_cast<std::int64_t>(run.income[owner]));
    run.checkpoint();
    const auto e3_log = sim::write_log(rec.log());
    REQUIRE(e3_log.ok());
    expansion_test::publish("e3-genuine.log", e3_log.value());
    expansion_test::publish("receiving-city.save", w.save());
    REQUIRE(w.check_invariants().empty());
    MESSAGE("EXPANSION_E3 tick " << w.clock().tick() << " source " << ref.island.v << ","
                                 << ref.city.v << " destination " << destination_ref.island.v << ","
                                 << destination_ref.city.v << " freight " << transferred
                                 << " receiver_height " << static_cast<unsigned>(*level) << " gold "
                                 << gold_of(w, 0) << " construction " << run.construction[0]
                                 << " upkeep " << run.upkeep[0]);
    // E4: a distinct later batch must require output made AFTER the first source export.
    REQUIRE(first_pipeline / 256 < std::numeric_limits<std::uint32_t>::max());
    const auto second_tons = static_cast<std::uint32_t>(first_pipeline / 256 + 1);
    const auto second_quantity = std::uint64_t{second_tons} * 256;
    REQUIRE(second_quantity > first_pipeline);
    expansion_test::sail(run, recipe.source);
    for (std::uint32_t n = 0; n < 400 && (expansion_test::stock(w, ref, wood) < second_quantity ||
                                          stats.forestry.farms.at(forest).made_88 - first_made <
                                              second_quantity - first_pipeline);
         ++n) {
        run.advance();
        expansion_test::source_ledger(w, ref, warehouse, forest, wood, stats, transferred);
    }
    const auto additionally_made = stats.forestry.farms.at(forest).made_88 - first_made;
    REQUIRE(additionally_made >= second_quantity - first_pipeline);
    REQUIRE(expansion_test::stock(w, ref, wood) >= second_quantity);
    const auto source_before_second_load = expansion_test::stock(w, ref, wood);
    const auto destination_before_second = command_test::city(w, destination_ref).stock.stock;
    run.native_cut("05-return-before-second-export", rs.types);
    auto second_load_before = w;
    second_load_before.set_transport_stats(nullptr);
    const sim::LoadCmd second_request{sim::FigureId{1}, warehouse, 0, wood, second_tons};
    const auto changed_entry = rec.log().entries.size();
    const auto second_loaded = run.apply({sim::PlayerId{0}, second_request});
    REQUIRE(second_loaded.ok());
    REQUIRE(second_loaded.records.at(0).transferred_88 == second_quantity);
    REQUIRE(cargo_oracle::load_effect(second_load_before, w, sim::PlayerId{0}, second_request, ref,
                                      0, second_quantity) == cargo_oracle::Difference::None);
    exported_total += second_quantity;
    REQUIRE(second_tons > 1);
    auto legal_changed = second_load_before;
    legal_changed.set_transport_stats(nullptr);
    auto changed_request = second_request;
    --changed_request.tons;
    const auto changed_answer = legal_changed.apply({sim::PlayerId{0}, changed_request});
    REQUIRE(changed_answer.ok());
    REQUIRE(changed_answer.records.at(0).transferred_88 ==
            std::uint64_t{changed_request.tons} * 256);
    REQUIRE(legal_changed.state_hash() != w.state_hash());
    auto second_held = end_holds;
    second_held[0].amount_88 += second_quantity;
    CHECK(ship_of(w, 1).holds == second_held);
    CHECK(expansion_test::stock(w, ref, wood) == source_before_second_load - second_quantity);
    CHECK(command_test::city(w, destination_ref).stock.stock == destination_before_second);
    expansion_test::source_ledger(w, ref, warehouse, forest, wood, stats,
                                  transferred + second_quantity);
    run.checkpoint();
    expansion_test::sail(run, recipe.destination);
    const auto source_before_second_unload = command_test::city(w, ref).stock.stock;
    const auto destination_before_unload = expansion_test::stock(w, destination_ref, wood);
    const auto second_unloaded = run.apply(
        {sim::PlayerId{0}, sim::UnloadCmd{sim::FigureId{1}, destination_wh, 0, wood, second_tons}});
    REQUIRE(second_unloaded.ok());
    REQUIRE(second_unloaded.records.at(0).transferred_88 == second_quantity);
    CHECK(ship_of(w, 1).holds == end_holds);
    CHECK(expansion_test::stock(w, destination_ref, wood) ==
          destination_before_unload + second_quantity);
    CHECK(command_test::city(w, ref).stock.stock == source_before_second_unload);
    expansion_test::source_ledger(w, ref, warehouse, forest, wood, stats,
                                  transferred + second_quantity);
    REQUIRE(aboard_of(ship_of(w, 1).holds, wood) >= 0);
    CHECK(std::uint64_t{12800} + stats.forestry.farms.at(forest).made_88 - 25 * 256 ==
          static_cast<std::uint64_t>(aboard_of(ship_of(w, 1).holds, wood)) +
              expansion_test::stock(w, ref, wood) +
              expansion_test::stock(w, destination_ref, wood) +
              std::get<sim::Farm>(w.find_building(forest)->payload).out +
              expansion_test::cart_wood(w, warehouse, wood));
    CHECK(command_test::city(w, ref).accounts.civil_cost == 27);
    CHECK(command_test::city(w, destination_ref).accounts.civil_cost == 20);
    REQUIRE(run.city_upkeep.at(ref) > 0);
    REQUIRE(run.city_upkeep.at(destination_ref) > 0);
    for (const auto& [owner, cash] : initial_money)
        CHECK(expansion_test::balances(w).at(owner) ==
              cash - static_cast<std::int64_t>(run.construction[owner] + run.upkeep[owner]) +
                  static_cast<std::int64_t>(run.income[owner]));
    run.checkpoint();
    const auto e4_log = sim::write_log(rec.log());
    REQUIRE(e4_log.ok());
    expansion_test::publish("e4-genuine.log", e4_log.value());
    expansion_test::publish("two-cycles.save", w.save());
    REQUIRE(w.check_invariants().empty());
    MESSAGE("EXPANSION_E4 tick " << w.clock().tick() << " old_pipeline " << first_pipeline
                                 << " second_export " << second_quantity << " additionally_made "
                                 << additionally_made << " source_wood "
                                 << expansion_test::stock(w, ref, wood) << " destination_wood "
                                 << expansion_test::stock(w, destination_ref, wood) << " gold "
                                 << gold_of(w, 0) << " construction " << run.construction[0]
                                 << " upkeep " << run.upkeep[0] << " source_upkeep "
                                 << run.city_upkeep.at(ref) << " destination_upkeep "
                                 << run.city_upkeep.at(destination_ref));
    REQUIRE(run.natives.size() == 5);
    const auto& honest = rec.log();
    auto changed = honest;
    REQUIRE(changed.entries.at(changed_entry).commands.size() == 1);
    auto* changed_load =
        std::get_if<sim::LoadCmd>(&changed.entries[changed_entry].commands[0].body);
    REQUIRE(changed_load != nullptr);
    REQUIRE(changed_load->tons == second_tons);
    --changed_load->tons;
    for (std::size_t i = 0; i < changed_entry; ++i)
        CHECK(changed.entries[i] == honest.entries[i]);
    REQUIRE(honest.entries.at(changed_entry + 1).kind == sim::ReplayEntry::Kind::Checkpoint);
    const auto normal_bytes = sim::write_log(honest);
    const auto changed_bytes = sim::write_log(changed);
    REQUIRE(normal_bytes.ok());
    REQUIRE(changed_bytes.ok());
    expansion_test::publish("expansion.log", normal_bytes.value());
    expansion_test::publish("expansion-changed-command.log", changed_bytes.value());
    std::size_t checkpoints = 0, prior = 0;
    for (std::size_t i = 0; i < honest.entries.size(); ++i)
        if (honest.entries[i].kind == sim::ReplayEntry::Kind::Checkpoint) {
            ++checkpoints;
            if (i < changed_entry)
                ++prior;
        }
    REQUIRE(checkpoints == 13);
    REQUIRE(prior == 10);
    CHECK(run.city_upkeep.at(ref) == 90);
    CHECK(run.city_upkeep.at(destination_ref) == 47);
    CHECK(gold_of(w, 0) == 28863);
    for (const auto& island : w.islands())
        if (island)
            for (const auto& city : island->cities)
                if (city) {
                    CHECK(city->accounts.sales == 0);
                    CHECK(city->accounts.trade_sales == 0);
                    CHECK(city->accounts.trade_purchases == 0);
                    CHECK(std::all_of(city->billed.begin(), city->billed.end(),
                                      [](auto value) { return value == 0; }));
                }
    std::ostringstream evidence;
    evidence << "state=134 schema=ECONBK1 PRNG=5 replay=4 command=1 save=1\n"
             << "tick=" << w.clock().tick() << " entries=" << honest.entries.size()
             << " checkpoints=" << checkpoints << " changed_load_entry=" << changed_entry
             << " changed_checkpoint=" << changed_entry + 1
             << " matched_prior_checkpoints=" << prior << " load_tons=" << second_tons
             << " changed_tons=" << changed_request.tons << "\n";
    for (const auto& native : run.natives) {
        REQUIRE(native.advances > 0);
        REQUIRE(native.commands > 0);
        CHECK(native.world.save() == w.save());
        evidence << "native=" << native.label << " suffix_advances=" << native.advances
                 << " suffix_commands=" << native.commands
                 << " final_hash=" << native.world.state_hash() << "\n";
    }
    for (std::size_t i = 0; i < run.draws.size(); ++i)
        evidence << "draw=" << sim::draw_site_name(static_cast<sim::DrawSite>(i))
                 << " advance_count=" << run.draws[i] << "\n";
    evidence << "direct_CityName=2 (one cursor draw per successful Found, between advances)\n";
    const auto& end_farm = std::get<sim::Farm>(w.find_building(forest)->payload);
    std::uint64_t active_raw = 0;
    for (const auto& actor : w.figures().list.slots)
        if (actor && !actor->expiry_stamp && actor->home == forest && actor->harvest)
            active_raw += actor->cargo_88;
    evidence << "cuts=" << observed_cuts << " growth=" << observed_growth
             << " raw_credited=" << stats.forestry.raw_credited_88
             << " raw_delivered=" << stats.forestry.raw_delivered_88 << " active_raw=" << active_raw
             << " consumed=" << stats.forestry.farms.at(forest).consumed_88
             << " farm_raw=" << end_farm.raw << " made=" << stats.forestry.farms.at(forest).made_88
             << " farm_out=" << end_farm.out
             << " active_wood_cart=" << expansion_test::cart_wood(w, warehouse, wood)
             << " source_stock=" << expansion_test::stock(w, ref, wood)
             << " exported=" << exported_total
             << " destination_stock=" << expansion_test::stock(w, destination_ref, wood) << "\n";
    for (const auto& [owner, cash] : initial_money)
        evidence << "owner=" << owner << " start=" << cash
                 << " construction=" << run.construction[owner]
                 << " resident_income=" << run.income[owner] << " city_upkeep=" << run.upkeep[owner]
                 << " player_upkeep=0 final=" << expansion_test::balances(w).at(owner)
                 << " residual=0\n";
    for (const auto city_ref : {ref, destination_ref}) {
        const auto& city = command_test::city(w, city_ref);
        evidence << "city=" << city_ref.island.v << "," << city_ref.city.v
                 << " stamp=" << city.accounts.stamp << " civil=" << city.accounts.civil_cost
                 << " military=" << city.accounts.military_cost
                 << " upkeep=" << run.city_upkeep.at(city_ref) << "\n";
    }
    const auto text = evidence.str();
    expansion_test::publish("continuation.txt",
                            std::vector<std::uint8_t>(text.begin(), text.end()));
    if (const char* expected = std::getenv("ANNO_EXPAND_E4_EXPECTED_FINAL")) {
        const auto expected_final_bytes = read_all(expected);
        REQUIRE_FALSE(expected_final_bytes.empty());
        CHECK(w.save() == expected_final_bytes);
        MESSAGE("EXPANSION_E4 trajectory byte-identical under full native monitoring");
    }
    MESSAGE("EXPANSION_E5 checkpoints " << checkpoints << " changed_entry " << changed_entry
                                        << " divergence_checkpoint " << changed_entry + 1
                                        << " matched_prior " << prior << " native_full_suffixes "
                                        << run.natives.size());
    w.set_transport_stats(nullptr);
}

TEST_CASE("expansion founding accounting: public founding retains prepared warehouse history") {
    // Relocated original-data component, not the genuine voyage. Compatibility disposition:
    // acbe5a24 GAMEPLAY-IDENTITY-134-135. Preserve the actual buggy133 files; never repair on load.
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto original = real_start(*dir);
    const auto city_of_warehouse = [](const sim::World& world, sim::BuildingId id) {
        const auto island = sim::island_of(id);
        REQUIRE(island.valid());
        REQUIRE(island.v < world.islands().size());
        REQUIRE(world.islands()[island.v].has_value());
        const auto& by_id = world.islands()[island.v]->cache.by_id;
        const auto found = by_id.find(id);
        REQUIRE(found != by_id.end());
        return found->second.city;
    };
    const auto publish = [](const char* name, const std::vector<std::uint8_t>& bytes) {
        if (const char* output = std::getenv("ANNO_EXPAND_OUT_DIR"); output && *output) {
            const auto path = std::filesystem::path(output) / name;
            const auto error = export_log_if_requested(path.string().c_str(), bytes);
            REQUIRE_MESSAGE(error.empty(), error);
        }
    };
    for (const bool enabled : {false, true}) {
        INFO("booking " << enabled);
        auto rules = original.rules;
        rules.city_booking = enabled;
        rules.save_billed_goods = true;
        auto made = sl::load_ship_start(original.manifest, *dir, original.types, {}, rules);
        REQUIRE_MESSAGE(made.ok(), (made.ok() ? std::string{} : made.error()));
        auto w = std::move(made).value();
        struct Recipe {
            SailLeg source, destination;
        };
        const Recipe recipe{{0, 1, {0, 255, 138}, 257, 134}, {0, 1, {1, 456, 353}, 452, 349}};
        const auto leg = recipe.source;
        sim::WorldTestAccess::relocate(w, sim::FigureId{1},
                                       {leg.goal_x * 256 + 128, leg.goal_y * 256 + 128});
        const auto pristine = w.save();
        auto wrong_owner = found_command(leg);
        wrong_owner.issuer = sim::PlayerId{1};
        CHECK(w.apply(wrong_owner).status == sim::Reject::NotShipOwner);
        CHECK(w.save() == pristine);
        (void)w.drain_events();
        const auto initial_holds = ship_of(w, 1).holds;
        const auto cursor = w.rng().cursor();
        auto recording = sim::Recorder::start(w);
        REQUIRE(recording.ok());
        auto rec = std::move(recording).value();
        const auto answer = rec.submit(found_command(leg));
        REQUIRE(answer.ok());
        rec.checkpoint();
        const auto wh = answer.records.at(0).building;
        const auto ref = city_of_warehouse(w, wh);
        const auto* building = w.find_building(wh);
        REQUIRE(building != nullptr);
        REQUIRE(std::holds_alternative<sim::Market>(building->payload));
        const auto* type = w.types().find(building->type);
        REQUIRE(type != nullptr);
        REQUIRE(type->running_cost == std::vector<std::uint32_t>{0, 15});
        const auto cost =
            sim::building_cost(w.types(), *type, *building, w.players().find(sim::PlayerId{0}));
        REQUIRE(cost.ok());
        REQUIRE(cost.value().active);
        REQUIRE(cost.value().kind == 1);
        REQUIRE(cost.value().amount == 15);
        CHECK(command_test::city(w, ref).accounts.civil_cost == (enabled ? 15 : 0));
        CHECK(command_test::city(w, ref).accounts.military_cost == 0);
        CHECK(w.players().find(sim::PlayerId{0})->upkeep_a == 0);
        CHECK(w.players().find(sim::PlayerId{0})->upkeep_b == 0);
        CHECK(gold_of(w, 0) == 29650);
        auto expected_holds = initial_holds;
        expected_holds[0].amount_88 -= 12 * 256;
        expected_holds[2].amount_88 -= 5 * 256;
        CHECK(ship_of(w, 1).holds == expected_holds);
        CHECK(w.rng().cursor() == (cursor + 1) % w.rng().size()); // one original city-name draw
        const auto cut = w.save();
        publish(enabled ? "current136-enabled-founded.save" : "current136-default-off-founded.save",
                cut);
        auto native = sim::World::load(cut, original.types);
        REQUIRE(native.ok());
        CHECK(native.value().save() == cut);

        // Optional external historical originals are required by the focused operator gate.
        // They were produced/round-tripped at e1369d7a before the correction. They remain133.
        if (const char* archive = std::getenv("ANNO_EXPAND_LEGACY133_DIR")) {
            const auto path =
                std::filesystem::path(archive) /
                (enabled ? "actual133-enabled-founded.save" : "actual133-default-off-founded.save");
            std::ifstream file(path, std::ios::binary);
            REQUIRE_MESSAGE(file.good(), path.string());
            const std::vector<std::uint8_t> old{std::istreambuf_iterator<char>(file), {}};
            const auto refused = sim::World::load(old, original.types);
            REQUIRE_FALSE(refused.ok());
            CHECK(refused.error() ==
                  "save: state stream version 133 is not supported (this build reads 136)");
            auto old_log =
                rec.log(); // current outer identity: the embedded native is the sole mismatch
            old_log.initial_save = old;
            auto clean = sim::World::load(rec.log().initial_save, original.types);
            REQUIRE(clean.ok());
            const auto before = clean.value().save();
            auto replay_refused = sim::replay(clean.value(), old_log);
            REQUIRE_FALSE(replay_refused.ok());
            CHECK(replay_refused.error() ==
                  "replay refused: initial save state stream version differs from this build's");
            CHECK(clean.value().save() == before);
            // Diagnostic byte transcript ONLY, never loaded or saved as a corrected artifact.
            // With the original PRNG table the first header starts at29. Field layout is fixed.
            REQUIRE(old.at(20) == 0);
            auto normalized = old;
            island_test::put32(normalized, 8, 134);
            island_test::put32(normalized, 29, 134);
            island_test::reseal(normalized);
            if (!enabled) {
                CHECK(normalized == cut); // default-off gameplay differs by identity only
            } else {
                REQUIRE(normalized.size() == cut.size());
                std::size_t changes = 0;
                for (std::size_t i = 0; i < cut.size() - 8; ++i)
                    if (cut[i] != normalized[i]) {
                        ++changes;
                        CHECK(normalized[i] == 0);
                        CHECK(cut[i] == 15); // only the missing prepared civil-cost contribution
                    }
                CHECK(changes == 1);
            }
            MESSAGE("valid historical133 original refused; identity-only off / history15 on "
                    "distinguished");
        }
        (void)w.drain_events();
        rec.advance(4000); // 2000ticks: one due city booking
        native.value().advance(4000);
        CHECK(native.value().save() == w.save());
        CHECK(native.value().last_output().events == w.last_output().events);
        REQUIRE(native.value().last_output().draws.size() == w.last_output().draws.size());
        for (std::size_t i = 0; i < w.last_output().draws.size(); ++i) {
            CHECK(native.value().last_output().draws[i].site == w.last_output().draws[i].site);
            CHECK(native.value().last_output().draws[i].count == w.last_output().draws[i].count);
        }
        CHECK(gold_of(w, 0) == (enabled ? 29649 : 29650));
        std::uint64_t charges = 0;
        for (const auto& event : w.drain_events())
            if (event.booking) {
                CHECK(event.booking->city == ref);
                CHECK(event.booking->income == 0);
                CHECK(event.booking->upkeep == 1);
                charges += event.booking->upkeep;
            }
        CHECK(charges == (enabled ? 1 : 0));
        rec.checkpoint();
        const auto bytes = sim::write_log(rec.log());
        REQUIRE(bytes.ok());
        const auto decoded = sim::read_log(bytes.value());
        REQUIRE(decoded.ok());
        auto fresh = sim::World::load(decoded.value().initial_save, original.types);
        REQUIRE(fresh.ok());
        const auto replayed = sim::replay(fresh.value(), decoded.value());
        REQUIRE(replayed.ok());
        REQUIRE(replayed.value().reproduced());
        CHECK(fresh.value().save() == w.save());
        publish(enabled ? "current136-enabled-found.log" : "current136-off-found.log",
                bytes.value());
        const auto gold = gold_of(w, 0);
        w.advance(0);
        CHECK(gold_of(w, 0) == gold);
        for (const auto& event : w.drain_events())
            CHECK_FALSE(event.booking.has_value()); // no double charge at the same logical tick

        // Additional public lifecycle/city-isolation checks remain in this labelled short fixture.
        const auto tools = *w.types().goods_by_name("WERKZEUG");
        const auto wood = *w.types().goods_by_name("HOLZ");
        REQUIRE(
            w.apply({sim::PlayerId{0}, sim::UnloadCmd{sim::FigureId{1}, wh, 2, tools, 2}}).ok());
        REQUIRE(w.apply(command_test::build(0, ref, {command_test::at(sim::TypeId{801}, 238, 128)}))
                    .ok());
        CHECK(command_test::city(w, ref).accounts.civil_cost == (enabled ? 27 : 0));
        REQUIRE(
            w.apply({sim::PlayerId{0}, sim::SetProductionLineCmd{{238, 128}, wood, false}}).ok());
        CHECK(command_test::city(w, ref).accounts.civil_cost == (enabled ? 19 : 0));
        sim::WorldTestAccess::relocate(
            w, sim::FigureId{1},
            {recipe.destination.goal_x * 256 + 128, recipe.destination.goal_y * 256 + 128});
        const auto second = w.apply(found_command(recipe.destination));
        REQUIRE(second.ok());
        const auto other = city_of_warehouse(w, second.records.at(0).building);
        REQUIRE(other.island != ref.island);
        CHECK(command_test::city(w, other).accounts.civil_cost == (enabled ? 15 : 0));
        CHECK(command_test::city(w, ref).accounts.civil_cost == (enabled ? 19 : 0));
        (void)w.drain_events();
        const auto before_two = gold_of(w, 0);
        w.advance(4000);
        CHECK(gold_of(w, 0) == before_two - (enabled ? 2 : 0));
        std::map<sim::CityRef, std::uint64_t> by_city;
        for (const auto& event : w.drain_events())
            if (event.booking)
                by_city[event.booking->city] += event.booking->upkeep;
        if (enabled) {
            CHECK(by_city[ref] == 1);
            CHECK(by_city[other] == 1);
            CHECK(by_city.size() == 2);
        } else
            CHECK(by_city.empty());
        CHECK(w.check_invariants().empty());
    }
}
