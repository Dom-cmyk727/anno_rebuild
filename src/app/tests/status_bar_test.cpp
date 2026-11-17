// UI-STATUS-1 B: the status bar's choice for the map tile under the cursor (status_bar.hpp;
// docs/re/ui.md 5.6, the owner's OQ-703 answer "Like original"). Synthetic territories, no game
// files: the rule is a pure function of the published TerritoryView and the tile.
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/status_bar.hpp>
#include <anno/app/ui_layout.hpp>
#include <anno/app/ui_mock.hpp>

namespace fs = std::filesystem;
namespace app = anno::app;
namespace sim = anno::sim;

namespace {

/// The original's per-climate masks (AnnoFrame jump table 0x10009904; SimConfig::links).
const std::vector<std::uint32_t> kMasks = {0x18000, 0xf009, 0xf949, 0xf06b, 0xf6b9, 0xf04d, 0xf04d};
constexpr std::uint32_t kNeutral = 15;
constexpr std::uint32_t kLocal = 1;
constexpr std::uint32_t kOther = 2;

/// A 16 x 8 NORD island in slot 3 at (100, 200): tiles x 100..107 are city 0 (the local
/// player's), x 108..111 city 1 (another player's), x 112..115 no city; the bottom row is sea.
app::TerritoryView world() {
    app::TerritoryView v;
    v.neutral_owner = kNeutral;
    v.climate_mask = kMasks;
    app::TerritoryIsland is;
    is.index = 3;
    is.x0 = 100;
    is.y0 = 200;
    is.w = 16;
    is.h = 8;
    is.climate = 2; // NORD
    is.owner.assign(std::size_t{16} * 8, kNeutral);
    auto land = std::make_shared<std::vector<std::uint8_t>>(std::size_t{16} * 8, 1);
    for (std::uint32_t y = 0; y < 8; ++y) {
        for (std::uint32_t x = 0; x < 16; ++x) {
            const std::size_t i = std::size_t{y} * 16 + x;
            is.owner[i] = x < 8 ? 0U : x < 12 ? 1U : kNeutral;
            if (y == 7) {
                (*land)[i] = 0;
            }
        }
    }
    is.land = std::move(land);
    is.cities.push_back(app::TerritoryCity{0, kLocal, {kLocal}});
    is.cities.push_back(app::TerritoryCity{1, kOther, {kOther}});
    is.discovered_by = {kLocal, kOther};
    v.islands.push_back(std::move(is));
    return v;
}

sim::CityRef city(std::uint32_t island, std::uint32_t no) {
    return sim::CityRef{sim::IslandId{island}, sim::CityNo{no}};
}

} // namespace

TEST_CASE("status bar: the fertility segments of every climate follow AnnoFrame 0x10009870") {
    using V = std::vector<std::uint32_t>;
    CHECK(app::fertility_segments(0, kMasks) == V{});            // POLAR 0x18000
    CHECK(app::fertility_segments(1, kMasks) == V{});            // TUNDRA 0xf009
    CHECK(app::fertility_segments(2, kMasks) == V{6, 8, 11});    // NORD: the GOG shots' row
    CHECK(app::fertility_segments(3, kMasks) == V{1, 5, 6});     // PRAERIE
    CHECK(app::fertility_segments(4, kMasks) == V{4, 5, 9, 10}); // DSCHUNGEL: bit 7 skipped
    CHECK(app::fertility_segments(5, kMasks) == V{2, 6});        // STEPPE
    CHECK(app::fertility_segments(6, kMasks) == V{2, 6});        // WUESTE
    CHECK(app::fertility_segments(7, kMasks) == V{});            // past the table
    // Controls: without the always-set filter NORD would start with 0 and 3 (the shot refutes
    // it), and three slots cut DSCHUNGEL's fourth segment.
    app::StatusBarRule no_filter;
    no_filter.always_set_bits = 0;
    CHECK(app::fertility_segments(2, kMasks, no_filter) == V{0, 3, 6, 8});
    app::StatusBarRule three;
    three.slots = 3;
    CHECK(app::fertility_segments(4, kMasks, three) == V{4, 5, 9});
}

TEST_CASE("status bar: own territory shows materials, residents and the fertility row") {
    const auto v = world();
    const auto p = app::pick_status(v, sim::TileXY{103, 202}, kLocal);
    REQUIRE(p.island.has_value());
    CHECK(*p.island == 3);
    CHECK(p.city == city(3, 0));
    CHECK(p.counters == app::StatusCounters::MaterialsResidents);
    CHECK(p.counters_city == city(3, 0));
    CHECK(p.fertility == app::FertilityAction::Show);
    CHECK(p.segments == std::vector<std::uint32_t>{6, 8, 11});
    CHECK(app::status_counters_name(p.counters) == "materials+residents");
    CHECK(app::status_island_text(p) == "3");
    CHECK(app::status_city_text(p.city) == "3:0");
}

TEST_CASE("status bar: own island outside the territory shows gold and the fertility row") {
    const auto v = world();
    const auto p = app::pick_status(v, sim::TileXY{113, 202}, kLocal);
    CHECK(p.island == std::optional<std::uint32_t>(3));
    CHECK_FALSE(p.city.has_value());
    CHECK(p.counters == app::StatusCounters::Gold);
    CHECK_FALSE(p.counters_city.has_value());
    CHECK(p.fertility == app::FertilityAction::Show);
}

TEST_CASE("status bar: a foreign city shows its residents only when it knows the local player") {
    auto v = world();
    const auto unknown = app::pick_status(v, sim::TileXY{109, 202}, kLocal);
    CHECK(unknown.city == city(3, 1));
    CHECK(unknown.counters == app::StatusCounters::Gold);
    CHECK_FALSE(unknown.counters_city.has_value());
    v.islands[0].cities[1].known_by = {kLocal, kOther};
    const auto known = app::pick_status(v, sim::TileXY{109, 202}, kLocal);
    CHECK(known.counters == app::StatusCounters::Residents);
    CHECK(known.counters_city == city(3, 1));
    // The fertility row is the island's, not the city's.
    CHECK(known.fertility == app::FertilityAction::Show);
}

TEST_CASE("status bar: open sea shows gold and hides the row at once") {
    const auto v = world();
    const auto p = app::pick_status(v, sim::TileXY{50, 50}, kLocal);
    CHECK_FALSE(p.island.has_value());
    CHECK(p.counters == app::StatusCounters::Gold);
    CHECK(p.fertility == app::FertilityAction::HideNow);
    CHECK(app::status_island_text(p) == "sea");
    const auto none = app::pick_status(v, std::nullopt, kLocal);
    CHECK(app::status_island_text(none) == "none");
    CHECK(app::status_tile_text(none.tile) == "none");
    CHECK(none.fertility == app::FertilityAction::HideNow);
}

TEST_CASE("status bar: sea inside a discovered island hides the row after the delay") {
    const auto v = world();
    // (113, 207): bottom row, no land, no city.
    const auto sea = app::pick_status(v, sim::TileXY{113, 207}, kLocal);
    CHECK(sea.island == std::optional<std::uint32_t>(3));
    CHECK(sea.fertility == app::FertilityAction::HideLater);
    const auto land = app::pick_status(v, sim::TileXY{113, 206}, kLocal);
    REQUIRE(land.fertility == app::FertilityAction::Show);

    app::FertilityRow row;
    row.update(land, 1000);
    CHECK(row.shown() == std::vector<std::uint32_t>{6, 8, 11});
    row.update(sea, 1100); // the delay starts
    CHECK(row.hide_at() == std::optional<std::uint64_t>(3100));
    CHECK_FALSE(row.shown().empty());
    row.update(sea, 3099); // one ms short: still shown, the delay is not restarted
    CHECK_FALSE(row.shown().empty());
    CHECK(row.hide_at() == std::optional<std::uint64_t>(3100));
    row.update(sea, 3100);
    CHECK(row.shown().empty());
    CHECK(app::fertility_text(row.shown()) == "hidden");
    row.update(sea, 3200); // still over that sea: no new delay for a hidden row
    CHECK_FALSE(row.hide_at().has_value());

    // Land again within the delay cancels the hide.
    app::FertilityRow back;
    back.update(land, 0);
    back.update(sea, 10);
    back.update(land, 1500);
    CHECK_FALSE(back.hide_at().has_value());
    back.update(sea, 2500);
    CHECK(back.hide_at() == std::optional<std::uint64_t>(4500));
    back.update(sea, 4000);
    CHECK(app::fertility_text(back.shown()) == "6,8,11");

    // Open sea hides at once, a pending delay included.
    app::FertilityRow now;
    now.update(land, 0);
    now.update(sea, 10);
    now.update(app::pick_status(v, sim::TileXY{50, 50}, kLocal), 20);
    CHECK(now.shown().empty());
    CHECK_FALSE(now.hide_at().has_value());
}

TEST_CASE("status bar: an undiscovered island hides the row; its counters do not test it") {
    auto v = world();
    v.islands[0].discovered_by = {kOther};
    const auto p = app::pick_status(v, sim::TileXY{113, 202}, kLocal);
    CHECK(p.island == std::optional<std::uint32_t>(3));
    CHECK(p.fertility == app::FertilityAction::HideNow);
    // The counters (0x10009920) never read the discovery word.
    const auto own = app::pick_status(v, sim::TileXY{103, 202}, kLocal);
    CHECK(own.counters == app::StatusCounters::MaterialsResidents);
    CHECK(own.fertility == app::FertilityAction::HideNow);
}

TEST_CASE("status bar: two own cities on one island show the territory's city") {
    auto v = world();
    v.islands[0].cities[1].owner = kLocal;
    CHECK(app::pick_status(v, sim::TileXY{103, 202}, kLocal).counters_city == city(3, 0));
    CHECK(app::pick_status(v, sim::TileXY{109, 202}, kLocal).counters_city == city(3, 1));
    CHECK(app::pick_status(v, sim::TileXY{109, 202}, kLocal).counters ==
          app::StatusCounters::MaterialsResidents);
}

TEST_CASE("status bar: an armed building's corners pick the first own city on the island") {
    auto v = world();
    // Tile (113, 202) has no city. A 6 x 2 footprint from (107, 202): corners (107, 202) city 0
    // (own) first.
    const auto own =
        app::pick_status(v, sim::TileXY{113, 202}, kLocal, sim::TileRect{107, 202, 6, 2});
    CHECK(own.counters == app::StatusCounters::MaterialsResidents);
    CHECK(own.counters_city == city(3, 0));
    CHECK_FALSE(own.city.has_value());
    // From (109, 202): corners in city 1 (another player's) and on no city: gold only.
    const auto foreign =
        app::pick_status(v, sim::TileXY{113, 202}, kLocal, sim::TileRect{109, 202, 4, 2});
    CHECK(foreign.counters == app::StatusCounters::Gold);
    // Order: (x0, y0) is not own, (x0, y1 - 1) is: the second corner gives the city.
    for (std::uint32_t x = 0; x < 16; ++x) {
        v.islands[0].owner[std::size_t{1} * 16 + x] = kNeutral; // row y = 201: no city
    }
    const auto second =
        app::pick_status(v, sim::TileXY{113, 202}, kLocal, sim::TileRect{103, 201, 1, 2});
    CHECK(second.counters_city == city(3, 0));
    // Over a city tile the corners are not asked (the tile's own city decides).
    const auto on_city =
        app::pick_status(v, sim::TileXY{109, 202}, kLocal, sim::TileRect{103, 202, 2, 2});
    CHECK(on_city.counters == app::StatusCounters::Gold);
}

TEST_CASE("status bar: a climate with no segment hides after the delay, not at once") {
    auto v = world();
    v.islands[0].climate = 1; // TUNDRA: count 0, the bar's 2 s timer (0x10063e5d)
    const auto p = app::pick_status(v, sim::TileXY{113, 202}, kLocal);
    CHECK(p.fertility == app::FertilityAction::HideLater);
    CHECK(p.segments.empty());
}

TEST_CASE("status bar: an island without a height column counts as land everywhere") {
    auto v = world();
    v.islands[0].land = nullptr;
    CHECK(app::pick_status(v, sim::TileXY{113, 207}, kLocal).fertility ==
          app::FertilityAction::Show);
}

TEST_CASE("status bar: hud.txt has the four fertility slots at x 547 + 40k with the shots' row") {
    const auto loaded = app::load_hud_layout(fs::path(ANNO_REPO_DIR) / "data" / "ui" / "hud.txt");
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    const app::HudLayout& hud = loaded.value();
    REQUIRE(app::fertility_slot_count(hud) == 4);
    const std::vector<std::optional<int>> shot_segments = {6, 8, 11, std::nullopt};
    for (std::size_t k = 0; k < 4; ++k) {
        const app::UiElement* el = hud.element(app::fertility_slot_name(k));
        REQUIRE(el != nullptr);
        CHECK(el->pos_x == 547 + 40 * static_cast<int>(k));
        CHECK(el->pos_y == 735);
        CHECK(el->tex_id == std::optional<int>(40));
        CHECK(el->segment == shot_segments[k]);
    }
    CHECK(hud.element("rosh_46") == nullptr);
}
