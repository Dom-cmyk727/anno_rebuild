// UI-PRODBAR-1 B3a: the fertility indicator's app-side parts (fertility_indicator.hpp; the rule in
// docs/re/ui.md 4.1). Hand-derived values from the GOG disassembly: the disc 0x10047a5b-0x10047d1a,
// the scaling 0x10047de4-0x10047dfa, the building check 0x1004799f-0x10047a21, the gates
// 0x10048464-0x100484e8 and 0x10047de0. The game-file cases skip without ANNO1503_GAME_DIR.
#include <cmath>
#include <cstdint>
#include <set>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/fertility_indicator.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/test/game_dir.hpp>

namespace app = anno::app;
namespace sim = anno::sim;

namespace {

/// An island big enough that no disc touches its edge.
constexpr app::TileRect kWide{-100, -100, 400, 400};

sim::BuildingType crop_type(sim::GoodsId input, std::uint32_t crop, std::uint32_t radius) {
    sim::BuildingType t;
    t.id = sim::TypeId{801};
    t.width = 3;
    t.height = 3;
    t.action_radius = radius;
    t.input_goods = {input};
    t.head_raw.assign(56, 0);
    t.head_raw[0x28] = static_cast<std::uint8_t>(crop & 0xffU);
    t.head_raw[0x29] = static_cast<std::uint8_t>(crop >> 8U);
    return t;
}

app::FertilityGates open_gates() {
    return app::FertilityGates{true, true, true, true, true};
}

/// Dry west of column `dry_below_x`; placeable everywhere. Counts the placement calls.
class HalfDry : public app::FertilityTileSource {
  public:
    explicit HalfDry(std::int32_t dry_below_x) : dry_below_x_(dry_below_x) {}
    bool crop_dry(sim::TileXY tile, std::uint32_t) const override { return tile.x < dry_below_x_; }
    bool crop_placeable(sim::TileXY, std::uint32_t) const override {
        ++placeable_calls;
        return true;
    }
    mutable std::size_t placeable_calls = 0;

  private:
    std::int32_t dry_below_x_;
};

} // namespace

TEST_CASE("fertility indicator: row half-widths are the original's midpoint-circle table") {
    CHECK(app::fertility_row_half_widths(3) == std::vector<std::uint32_t>{3, 3, 3, 2});
    CHECK(app::fertility_row_half_widths(4) == std::vector<std::uint32_t>{4, 4, 3, 3, 1});
    CHECK(app::fertility_row_half_widths(5) == std::vector<std::uint32_t>{5, 5, 5, 4, 3, 2});
    // r <= 3 widens rows k >= r - trunc((r + 1) / 4) by one: r 2 is 2,2,1 before, 2,2,2 after.
    CHECK(app::fertility_row_half_widths(2) == std::vector<std::uint32_t>{2, 2, 2});
    CHECK(app::fertility_row_half_widths(1) == std::vector<std::uint32_t>{1, 1});
}

TEST_CASE("fertility indicator: disc tile counts for a 3 x 3 footprint") {
    const sim::TileXY origin{10, 20};
    const auto count = [&](std::uint32_t r) {
        return app::fertility_area(origin, 3, 3, r, kWide).size();
    };
    CHECK(count(1) == 0);
    CHECK(count(2) == 16);
    CHECK(count(3) == 36);
    CHECK(count(4) == 52);
    CHECK(count(5) == 88);
    // Negative control: the forester's 11 x 11 work square without the footprint is 112, the
    // footprint grown by r = 5 (13 x 13 - 9) 160. The disc is neither.
    CHECK(count(5) != 112);
    CHECK(count(5) != 160);

    const auto tiles = app::fertility_area(origin, 3, 3, 5, kWide);
    const std::set<sim::TileXY> unique(tiles.begin(), tiles.end());
    CHECK(unique.size() == tiles.size()); // no tile twice (the down row is skipped at k = 0)
    for (const sim::TileXY t : tiles) {
        CAPTURE(t.x);
        CAPTURE(t.y);
        const bool in_footprint = t.x >= 10 && t.x < 13 && t.y >= 20 && t.y < 23;
        CHECK_FALSE(in_footprint);
        // Centred on the centre cell (11, 21), never further than r along an axis.
        CHECK(std::abs(t.x - 11) <= 5);
        CHECK(std::abs(t.y - 21) <= 5);
    }
    // The corner of the 11 x 11 square is not in the disc; the end of the middle row is.
    CHECK(unique.count(sim::TileXY{6, 16}) == 0);
    CHECK(unique.count(sim::TileXY{6, 21}) == 1);
}

TEST_CASE(
    "fertility indicator: an even footprint has a two-cell centre, and rotation swaps W and H") {
    // 4 x 3: cx0 = x0 + 1, cx1 = x0 + 2; rows as for the 3 x 3 but one column wider.
    const auto even = app::fertility_area(sim::TileXY{0, 0}, 4, 3, 3, kWide);
    const auto odd = app::fertility_area(sim::TileXY{0, 0}, 3, 3, 3, kWide);
    // The 4 rows outside the footprint gain one column each; the 3 footprint rows gain one column
    // and lose one more footprint tile: 36 + 4 = 40.
    CHECK(odd.size() == 36);
    CHECK(even.size() == 40);
    sim::BuildingType t = crop_type(0x46, 104, 3);
    t.width = 4;
    t.height = 3;
    const app::TileRect island{0, 0, 100, 100};
    HalfDry none(-1000);
    const auto r0 =
        app::fertility_decision(&t, sim::TileXY{40, 40}, 0, island, open_gates(), &none);
    const auto r1 =
        app::fertility_decision(&t, sim::TileXY{40, 40}, 1, island, open_gates(), &none);
    CHECK(r0.n == app::fertility_area(sim::TileXY{40, 40}, 4, 3, 3, island).size());
    CHECK(r1.n == app::fertility_area(sim::TileXY{40, 40}, 3, 4, 3, island).size());
}

TEST_CASE("fertility indicator: the island rectangle clamps the disc") {
    // Origin at the island's north-west corner: rows above y0 and columns left of x0 drop out.
    const app::TileRect island{10, 20, 50, 50};
    const auto tiles = app::fertility_area(sim::TileXY{10, 20}, 3, 3, 5, island);
    CHECK(tiles.size() < 88);
    for (const sim::TileXY t : tiles) {
        CHECK(t.x >= 10);
        CHECK(t.y >= 20);
    }
    // The clamped disc is exactly the unclamped one filtered to the island.
    const auto all = app::fertility_area(sim::TileXY{10, 20}, 3, 3, 5, kWide);
    std::size_t inside = 0;
    for (const sim::TileXY t : all) {
        inside += (t.x >= 10 && t.y >= 20) ? 1U : 0U;
    }
    CHECK(tiles.size() == inside);
}

TEST_CASE("fertility indicator: the value is trunc(10 times good / N)") {
    CHECK(app::fertility_value(59, 88) == 6);
    // Negative control: rounding would give 7.
    CHECK(std::llround(10.0 * 59 / 88) == 7);
    CHECK(app::fertility_value(88, 88) == 10);
    CHECK(app::fertility_value(87, 88) == 9);
    CHECK(app::fertility_value(0, 88) == 0);
    CHECK(app::fertility_value(0, 0) == 0);
    CHECK(app::fertility_value(18, 36) == 5);
}

TEST_CASE("fertility indicator: which types show it (synthetic types)") {
    // Forester: input BAUM 0x46, crop object 104.
    CHECK(app::fertility_crop_object(crop_type(0x46, 104, 5)) == std::optional<std::uint32_t>{104});
    // Fisher: input FISCH 0x47 is in range, but its crop object is 0.
    CHECK_FALSE(app::fertility_crop_object(crop_type(0x47, 0, 7)));
    // Hunter: input FLEISCHTIER 0x89 is outside 0x38..0x4b, even with a crop object.
    CHECK_FALSE(app::fertility_crop_object(crop_type(0x89, 104, 8)));
    // Range ends.
    CHECK(app::fertility_crop_object(crop_type(0x38, 712, 3)));
    CHECK(app::fertility_crop_object(crop_type(0x4b, 712, 3)));
    CHECK_FALSE(app::fertility_crop_object(crop_type(0x37, 712, 3)));
    CHECK_FALSE(app::fertility_crop_object(crop_type(0x4c, 712, 3)));
    // A u16 crop object above 255 reads both bytes (712 = 0x2c8).
    CHECK(app::fertility_crop_object(crop_type(0x38, 712, 3)) == std::optional<std::uint32_t>{712});
    // No input at all, a short HEAD.
    sim::BuildingType none = crop_type(0x46, 104, 5);
    none.input_goods.clear();
    CHECK_FALSE(app::fertility_crop_object(none));
    sim::BuildingType shorthead = crop_type(0x46, 104, 5);
    shorthead.head_raw.resize(0x28);
    CHECK_FALSE(app::fertility_crop_object(shorthead));
}

TEST_CASE("fertility indicator: 801 shows it, 810 and 805 do not (Anlagen.dat)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto loaded =
        anno::sim_load::load_original_registry(*dir, anno::sim_load::FigureEntries::OptionalNote);
    REQUIRE_MESSAGE(loaded.types != nullptr, loaded.error);
    const sim::TypeRegistry& reg = *loaded.types;
    const auto crop = [&](std::uint32_t id) {
        const sim::BuildingType* t = reg.find(sim::TypeId{id});
        REQUIRE_MESSAGE(t != nullptr, "type ", id);
        return app::fertility_crop_object(*t);
    };
    CHECK(crop(801) == std::optional<std::uint32_t>{104}); // forester
    CHECK(crop(811) == std::optional<std::uint32_t>{712}); // grain farm
    CHECK(crop(807) == std::optional<std::uint32_t>{1});   // sheep farm: object 1
    CHECK_FALSE(crop(810));                                // fisher: object 0
    CHECK_FALSE(crop(805));                                // hunter: input out of range
    // The radius the disc uses is HEAD[0x11].
    CHECK(reg.find(sim::TypeId{801})->action_radius == 5);
    CHECK(reg.find(sim::TypeId{811})->action_radius == 3);
}

TEST_CASE("fertility indicator: every gate closes the bar on its own") {
    const sim::BuildingType t = crop_type(0x46, 104, 5);
    HalfDry src(-1000);
    const sim::TileXY at{40, 40};
    const app::TileRect island{0, 0, 100, 100};
    struct Case {
        bool app::FertilityGates::* gate;
        const char* name;
    };
    const Case cases[] = {
        {&app::FertilityGates::preview_valid, "preview-valid"},
        {&app::FertilityGates::last_ghost, "last-ghost"},
        {&app::FertilityGates::rect_nonempty, "rect-nonempty"},
        {&app::FertilityGates::type_found, "type-found"},
        {&app::FertilityGates::projected, "projected"},
    };
    for (const Case& c : cases) {
        CAPTURE(c.name);
        app::FertilityGates g = open_gates();
        g.*c.gate = false;
        const auto d = app::fertility_decision(&t, at, 0, island, g, &src);
        CHECK(d.outcome == app::FertilityDecision::Outcome::GateClosed);
        CHECK(d.reason == c.name);
        CHECK(app::fertility_label(d) == std::string("gate:") + c.name);
    }
    // The projection is tested after the count, as at 0x10047de0: the count exists, no frame.
    app::FertilityGates g = open_gates();
    g.projected = false;
    const auto late = app::fertility_decision(&t, at, 0, island, g, &src);
    CHECK(late.n == 88);
    // The earlier gates stop before the count.
    g = open_gates();
    g.preview_valid = false;
    CHECK(app::fertility_decision(&t, at, 0, island, g, &src).n == 0);
    // A null type closes the registry gate even when the flag says found.
    CHECK(app::fertility_decision(nullptr, at, 0, island, open_gates(), &src).reason ==
          "type-found");
}

TEST_CASE("fertility indicator: without a tile source it is the named placeholder") {
    const sim::BuildingType t = crop_type(0x46, 104, 5);
    const auto d =
        app::fertility_decision(&t, sim::TileXY{40, 40}, 0, kWide, open_gates(), nullptr);
    CHECK(d.outcome == app::FertilityDecision::Outcome::NeedsSim);
    CHECK(app::fertility_label(d) == "fertility-indicator-needs-sim");
    // Not eligible wins over the missing source (the building check comes first).
    const sim::BuildingType fisher = crop_type(0x47, 0, 7);
    CHECK(app::fertility_decision(&fisher, sim::TileXY{40, 40}, 0, kWide, open_gates(), nullptr)
              .outcome == app::FertilityDecision::Outcome::NotEligible);
}

TEST_CASE("fertility indicator: an injected source gives the hand-counted frame") {
    const sim::BuildingType t = crop_type(0x46, 104, 5);
    // Footprint at (40, 40): centre column 41. Dry west of x = 41: the columns 36..40 of the disc.
    HalfDry src(41);
    const auto d = app::fertility_decision(&t, sim::TileXY{40, 40}, 0, kWide, open_gates(), &src);
    REQUIRE(d.outcome == app::FertilityDecision::Outcome::Draw);
    CHECK(d.n == 88);
    // Hand count: the disc's tiles with x >= 41, footprint excluded. Rows k = 0..5 have half-width
    // 5,5,5,4,3,2; per row the columns 41..41 + w: 6,6,6,5,4,3 for each of the 11 rows
    // (k = 0 once): 6 + 2 * (6 + 6 + 5 + 4 + 3) = 54; minus the footprint's x >= 41 tiles (2 x 3 =
    // 6) = 48.
    CHECK(d.good == 48);
    CHECK(d.frame == 5); // trunc(480 / 88)
    CHECK(app::fertility_label(d) == "frame 5");
    // The placement test runs only where the crop is not dry (0x10047c1b: jne past it).
    CHECK(src.placeable_calls == 48);
}

TEST_CASE("fertility indicator: the sprite is figure 1557, 11 frames of 54 x 12") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto frames = app::load_fertility_frames(*dir);
    REQUIRE_MESSAGE(frames.ok(), (frames.ok() ? std::string() : frames.error()));
    REQUIRE(frames.value().size() == app::kFertilityFrames);
    std::vector<std::vector<std::uint8_t>> group0;
    for (const auto& f : frames.value()) {
        REQUIRE(f.frames.size() == 3);
        for (const auto& g : f.frames) {
            REQUIRE(g.has_value());
            CHECK(g->frame.width == 54);
            CHECK(g->frame.height == 12);
            CHECK(g->frame.anchor_x == 27);
            CHECK(g->frame.anchor_y == 12);
        }
        group0.push_back(f.frames[0]->image->pixels);
    }
    // Every frame differs from the next: one more segment each.
    for (std::size_t k = 1; k < group0.size(); ++k) {
        CAPTURE(k);
        CHECK(group0[k] != group0[k - 1]);
    }
}

TEST_CASE("fertility indicator: blending puts the anchor pixel at the point and clips") {
    anno::assets::Rgba img;
    img.width = 2;
    img.height = 1;
    img.pixels = {255, 0, 0, 255, 0, 255, 0, 0}; // red opaque, green transparent
    std::vector<std::uint32_t> view(4 * 3, 0x00102030U);
    app::blend_fertility_frame(img, 1, 1, 2, 2, 4, 3, view.data()); // pixel (0,0) lands at (1,1)
    CHECK(view[1 * 4 + 1] == 0x00ff0000U);
    CHECK(view[1 * 4 + 2] == 0x00102030U); // transparent: unchanged
    // Partly outside: nothing written outside, no crash.
    app::blend_fertility_frame(img, 0, 0, -1, 0, 4, 3, view.data());
    CHECK(view[0] == 0x00102030U); // the red pixel fell at x = -1
}
