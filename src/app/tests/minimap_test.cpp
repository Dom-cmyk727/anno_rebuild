// UI-MINIMAP-1 step B1: the minimap's data, image, palette, geometry and drawing (minimap.hpp),
// against values computed by hand from docs/re/ui.md section 2.9's formulas (each case shows the
// arithmetic) and the shipped data/ui/minimap.txt. No original files needed, except the fix round's
// scroll drawing case (it skips without ANNO1503_GAME_DIR).
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/minimap.hpp>
#include <anno/app/ui_layout.hpp>
#include <anno/app/ui_mock.hpp>
#include <anno/test/game_dir.hpp>

namespace fs = std::filesystem;
namespace app = anno::app;

namespace {

/// A square world of `size` tiles with one island.
app::MinimapWorld one_island(std::uint32_t size, app::MinimapIsland island) {
    app::MinimapWorld w;
    w.w = size;
    w.h = size;
    w.islands.push_back(std::move(island));
    return w;
}

std::uint16_t cell(const app::MinimapConfig& cfg, const std::vector<std::uint16_t>& image,
                   std::uint32_t x, std::uint32_t y) {
    return image.at(std::size_t{y} * cfg.image_w + x);
}

std::array<std::uint8_t, 4> pixel(const anno::assets::Rgba& img, int x, int y) {
    const std::size_t o =
        (static_cast<std::size_t>(y) * img.width + static_cast<std::size_t>(x)) * 4;
    return {img.pixels.at(o), img.pixels.at(o + 1), img.pixels.at(o + 2), img.pixels.at(o + 3)};
}

} // namespace

TEST_CASE("minimap config: the shipped data/ui/minimap.txt states the original's values") {
    const auto loaded =
        app::load_minimap_config(fs::path(ANNO_REPO_DIR) / "data" / "ui" / "minimap.txt");
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? "" : loaded.error()));
    // MinimapConfig's defaults are the original's (each with its address in minimap.hpp); the
    // file must say the same, so neither drifts alone.
    CHECK(loaded.value() == app::MinimapConfig{});
    CHECK(loaded.value().colours.size() == 10);
    CHECK(loaded.value().image_w == 256);
}

TEST_CASE("minimap config: a file changes the numbers, a bad line is refused with its number") {
    const auto ok = app::parse_minimap_config("anno1503-ui-minimap 1\n# a comment\nimage 512 384\n"
                                              "colour 0 112233\ncolour 1 445566\nno_owner 1\n");
    REQUIRE(ok.ok());
    CHECK(ok.value().image_w == 512);
    CHECK(ok.value().image_h == 384);
    CHECK(ok.value().colours == std::vector<std::uint32_t>{0x112233, 0x445566});
    CHECK(ok.value().no_owner == 1);

    const auto unknown = app::parse_minimap_config("anno1503-ui-minimap 1\nimage 256 256\nfoo 1\n");
    REQUIRE_FALSE(unknown.ok());
    CHECK(unknown.error().find("line 3") != std::string::npos);
    CHECK_FALSE(app::parse_minimap_config("image 256 256\n").ok()); // no version
    CHECK_FALSE(app::parse_minimap_config("anno1503-ui-minimap 1\nimage 0 256\n").ok());
    CHECK_FALSE(app::parse_minimap_config("anno1503-ui-minimap 1\ncolour 1 ffffff\n").ok()); // gap
    CHECK_FALSE(app::parse_minimap_config("anno1503-ui-minimap 1\nno_owner 10\n").ok());
}

TEST_CASE("minimap palette: the original's 1:5:5:5 entries, shades 0 and 1 transparent") {
    // The GOG build's 16-bit frame (colour_depth 16; the History Edition's 32-bit entries: the
    // fix round's case below).
    app::MinimapConfig cfg;
    cfg.colour_depth = 16;
    const auto pal = app::minimap_palette(cfg);
    REQUIRE(pal.size() == 160);
    // Shades 0 and 1 of every colour draw nothing (AnnoFrame 0x10033943).
    for (std::uint32_t c = 0; c < 10; ++c) {
        CHECK(pal[c * 16 + 0][3] == 0);
        CHECK(pal[c * 16 + 1][3] == 0);
        CHECK(pal[c * 16 + 2][3] == 255);
    }
    // Red and blue show as (v * 255 + 15) / 31, green as the RGB565 frame's 6 bits g6 = (g << 1)
    // | (g >> 4), (g6 * 255 + 31) / 63.
    // (9, 8): b2ab00, factor 8 + 16 = 24. R: (178 + 4) << 7 = 23296, * 24 / 31 = 18035, >> 10 =
    // 17, (17 * 255 + 15) / 31 = 140. G: (171 + 4) << 2 = 700, * 24 / 31 = 541, >> 5 = 16, g6 =
    // 33 -> 134. B: (0 + 4) >> 3 = 0 -> 0.
    CHECK(pal[9 * 16 + 8] == std::array<std::uint8_t, 4>{140, 134, 0, 255});
    // (9, 3), factor 19: R 23296 * 19 / 31 = 14278 >> 10 = 13 -> 107; G 700 * 19 / 31 = 429 >> 5
    // = 13, g6 = 26 -> 105: (107, 105, 0), the commonest land colour of the GOG shot.
    CHECK(pal[9 * 16 + 3] == std::array<std::uint8_t, 4>{107, 105, 0, 255});
    // (9, 2) uses 664400, factor 18. R: 106 << 7 = 13568, * 18 / 31 = 7878, >> 10 = 7 -> 58. G:
    // 72 << 2 = 288, * 18 / 31 = 167, >> 5 = 5, g6 = 10 -> 40. B 0. (58, 40, 0) is in the shot.
    CHECK(pal[9 * 16 + 2] == std::array<std::uint8_t, 4>{58, 40, 0, 255});
    // (8, 15): fafafa, factor 31: every channel clamps to 31 (g6 63) -> 255.
    CHECK(pal[8 * 16 + 15] == std::array<std::uint8_t, 4>{255, 255, 255, 255});
    // (1, 15): 00bbff, factor 31. R: 4 << 7 = 512 -> 512 >> 10 = 0. G: 191 << 2 = 764 -> >> 5 =
    // 23, g6 = 47 -> 190. B: 259 >> 3 = 32, * 31 / 31 = 32 -> clamped 31 -> 255.
    CHECK(pal[1 * 16 + 15] == std::array<std::uint8_t, 4>{0, 190, 255, 255});
}

TEST_CASE("minimap image: owner x 16 + shade from a synthetic island") {
    const app::MinimapConfig cfg;
    // A 512-tile world: sx = sy = 2 tiles per cell. An island at (10, 20), 8 x 6 tiles, height
    // 100 everywhere; its owner 3 on its first column, else none.
    app::MinimapIsland is;
    is.x = 10;
    is.y = 20;
    is.w = 8;
    is.h = 6;
    is.heights.assign(48, 100);
    is.owners.assign(48, cfg.no_owner);
    for (std::uint32_t y = 0; y < 6; ++y)
        is.owners[y * 8] = 3;
    const auto image = app::build_minimap_image(cfg, one_island(512, is));
    REQUIRE(image.size() == 256 * 256);
    // Cell (5, 10): window tiles x [10, 14), y [20, 24), all island: 16 * 100 / (2 * 2 * 2 * 2) =
    // 100; (100 + 8) >> 4 = 6. Owner tile (trunc(5.5 * 2), trunc(10.5 * 2)) = (11, 21): island
    // column 1, no owner (9).
    CHECK(cell(cfg, image, 5, 10) == 9 * 16 + 6);
    // Cell (4, 10): window x [8, 12): two island columns of four: 8 * 100 / 16 = 50; (58) >> 4 =
    // 3. Owner tile (9, 21) is sea: 9.
    CHECK(cell(cfg, image, 4, 10) == 9 * 16 + 3);
    // Cell (5, 9): owner tile (11, 19) is above the island: 9. Window y [18, 22): rows 20, 21 of
    // four, x [10, 14) all island: 8 * 100 / 16 = 50 -> 3.
    CHECK(cell(cfg, image, 5, 9) == 9 * 16 + 3);
    // The sea: shade 0, no owner.
    CHECK(cell(cfg, image, 100, 100) == 9 * 16 + 0);

    // The owner: cell 5 samples tile x 11, so move the island's owned first column there.
    is.x = 11;
    const auto owned = app::build_minimap_image(cfg, one_island(512, is));
    // Cell (5, 10): owner tile (11, 21) = island column 0, owner 3. Window x [10, 14): 3 island
    // columns (11..13) of 4: 12 * 100 / 16 = 75; (83) >> 4 = 5.
    CHECK(owned.at(10 * 256 + 5) == 3 * 16 + 5);
}

TEST_CASE("minimap image: the nominal divisor and the nibble wrap") {
    const app::MinimapConfig cfg;
    // A 500-tile world: sx = 500 / 256 = 1.953125 (exact in float). Cell (1, 1): tiles
    // [trunc(1.953125), trunc(3 * 1.953125)) = [1, 5) on both axes, 16 tiles of 200 = 3200,
    // divided by the nominal 1.953125^2 * 4 = 15.2587890625 -> 209.71, trunc 209; (217) >> 4 =
    // 13 (not 200's 13 by luck: 3200 / 16 = 200 -> 13 as well, so check a cell where they part).
    app::MinimapIsland is;
    is.x = 0;
    is.y = 0;
    is.w = 20;
    is.h = 20;
    is.heights.assign(400, 200);
    auto image = app::build_minimap_image(cfg, one_island(500, is));
    CHECK(cell(cfg, image, 1, 1) == 9 * 16 + 13);
    // Cell (0, 0): tiles [0, 3) on both axes (trunc(2 * 1.953125) = 3): 9 tiles of 200 = 1800,
    // / 15.2587890625 = 117.96 -> 117; (125) >> 4 = 7. A true mean (200) would give 13.
    CHECK(cell(cfg, image, 0, 0) == 9 * 16 + 7);
    // Height 250 everywhere on a 512 world: v = 250, (258) >> 4 = 16, kept as 16 mod 16 = 0
    // (AnnoWelt 0x100a3e7a..0x100a3e90 keep the low nibble): transparent.
    is.heights.assign(400, 250);
    image = app::build_minimap_image(cfg, one_island(512, is));
    CHECK(cell(cfg, image, 2, 2) == 9 * 16 + 0);
}

TEST_CASE("minimap rectangle: at 1024 x 768 and in a wider layer") {
    const app::MinimapConfig cfg;
    // x = W - 1024/4 - 3 + 2, y = 12 - 2, 256 x 128 (docs/re/ui.md 2.9 Q1).
    CHECK(app::minimap_rect(cfg, 1024) == app::MinimapRect{767, 10, 256, 128, 1.0});
    // Anchored to the right edge (Q7): 2542 - 256 - 1 = 2285.
    CHECK(app::minimap_rect(cfg, 2542) == app::MinimapRect{2285, 10, 256, 128, 1.0});
}

TEST_CASE("minimap transform: map points to the screen at turn 0") {
    const app::MinimapConfig cfg;
    const auto rect = app::minimap_rect(cfg, 1024);
    const app::MinimapWindow whole;  // z = 1: [0, 1] on both axes
    const double h = std::sqrt(0.5); // cos 45 = sin 45
    // The centre (64, 64): du = dv = 0 -> (767 + 128, 10 + 64).
    auto p = app::minimap_screen(cfg, rect, whole, 64, 64);
    CHECK(p.first == doctest::Approx(895.0));
    CHECK(p.second == doctest::Approx(74.0));
    // (128, 64): du = 1, dv = 0 -> x = 895 + 88 sin 45, y = 74 + 44 cos 45.
    p = app::minimap_screen(cfg, rect, whole, 128, 64);
    CHECK(p.first == doctest::Approx(895.0 + 88.0 * h));
    CHECK(p.second == doctest::Approx(74.0 + 44.0 * h));
    // (0, 0): du = dv = -1 -> x = 895, y = 74 - 88 cos 45 (the diamond's top corner).
    p = app::minimap_screen(cfg, rect, whole, 0, 0);
    CHECK(p.first == doctest::Approx(895.0));
    CHECK(p.second == doctest::Approx(74.0 - 88.0 * h));
    // The window at z = 0.5 around the centre, and pushed back inside at an edge (0x10033710).
    const auto mid = app::minimap_window(0.5, 0.5, 0.5);
    CHECK(mid.u0 == 0.25);
    CHECK(mid.u1 == 0.75);
    const auto edge = app::minimap_window(0.5, 0.1, 0.95);
    CHECK(edge.u0 == 0.0);
    CHECK(edge.u1 == 0.5);
    CHECK(edge.v0 == 0.5);
    CHECK(edge.v1 == 1.0);
}

TEST_CASE("minimap view box: corners at turn 0 from the camera, the level and the world width") {
    const app::MinimapConfig cfg;
    const auto rect = app::minimap_rect(cfg, 1024);
    const app::MinimapWindow whole;
    // A 512 x 512 world, camera at tile (256, 256): P = 128 * 256 / 512 + 332.8 / 256 = 65.3 on
    // both axes. Level 0: e = 2200 / 512 = 4.296875, g = -0.3 e. Corners A (P - g, P + e), B (P +
    // e, P - g), C (P + g, P - e), D (P - e, P + g); each through du = (u - 64) / 64, dv likewise,
    // x = 895 - 88 (c dv - s du), y = 74 + 44 (c du + s dv), c = s = cos 45:
    // A (892.0756, 77.9795), B (897.9244, 77.9795), C (897.9244, 72.5484), D (892.0756, 72.5484).
    const auto box = app::minimap_view_box(cfg, rect, whole, 256, 256, 512, 512, 1.0);
    REQUIRE(box.size() == 4);
    const std::array<std::array<double, 4>, 4> want = {{{892.0756, 77.9795, 897.9244, 77.9795},
                                                        {897.9244, 77.9795, 897.9244, 72.5484},
                                                        {897.9244, 72.5484, 892.0756, 72.5484},
                                                        {892.0756, 72.5484, 892.0756, 77.9795}}};
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(box[i].x0 == doctest::Approx(want[i][0]).epsilon(1e-6));
        CHECK(box[i].y0 == doctest::Approx(want[i][1]).epsilon(1e-6));
        CHECK(box[i].x1 == doctest::Approx(want[i][2]).epsilon(1e-6));
        CHECK(box[i].y1 == doctest::Approx(want[i][3]).epsilon(1e-6));
    }
    // The pixels: floor(x + 0.5): x 892..898, y 73..78, the outline of a 7 x 6 rectangle.
    const auto px = app::minimap_line_pixels(box);
    CHECK(px.size() == 2 * 7 + 2 * 4);
    CHECK(px.front() == std::pair{892, 73});
    CHECK(px.back() == std::pair{898, 78});
    // Level 2 (the farthest of the original's three): e four times as large (0x1002ff5b):
    // A (883.3024, 86.1261), C (906.6976, 64.4019).
    const auto far = app::minimap_view_box(cfg, rect, whole, 256, 256, 512, 512, 4.0);
    REQUIRE(far.size() == 4);
    CHECK(far[0].x0 == doctest::Approx(883.3024).epsilon(1e-6));
    CHECK(far[0].y0 == doctest::Approx(86.1261).epsilon(1e-6));
    CHECK(far[2].x0 == doctest::Approx(906.6976).epsilon(1e-6));
    CHECK(far[2].y0 == doctest::Approx(64.4019).epsilon(1e-6));
    // At the world's edge the box is clipped to the window (0x1002fb70): camera at tile (0, 0):
    // P = 1.3; D's u = 1.3 - 4.297 < 0, so C-D and D-A end on u = 0 (the diamond's upper right
    // edge), and no point lies outside [0, 128].
    const auto clipped = app::minimap_view_box(cfg, rect, whole, 0, 0, 512, 512, 1.0);
    REQUIRE_FALSE(clipped.empty());
    for (const auto& s : clipped) {
        for (const auto& [x, y] : {std::pair{s.x0, s.y0}, std::pair{s.x1, s.y1}}) {
            // Inside the diamond, half-diagonals 2 * 88 c and 2 * 44 c (124.45 and 62.23):
            // |x - 895| / (176 c) + |y - 74| / (88 c) <= 1.
            const double h = std::sqrt(0.5);
            CHECK(std::abs(x - 895.0) / (176.0 * h) + std::abs(y - 74.0) / (88.0 * h) <=
                  doctest::Approx(1.0).epsilon(1e-9));
        }
    }
}

TEST_CASE("minimap compose: parchment under the map, the sea transparent, outside the diamond") {
    app::MinimapConfig cfg;
    cfg.image_w = 4;
    cfg.image_h = 4;
    // The GOG frame's parchment fit (B1): 256 x 128 at the rectangle + (1, 1), like the map.
    cfg.parchment_x0 = cfg.parchment_y0 = cfg.parchment_x1 = cfg.parchment_y1 = 1;
    const auto rect = app::minimap_rect(cfg, 1024);
    const auto palette = app::minimap_palette(cfg);
    // Every cell owner 9 shade 8, except cell (0, 0) the sea (shade 0).
    std::vector<std::uint16_t> image(16, 9 * 16 + 8);
    image[0] = 9 * 16;
    // A parchment of 256 x 133 in one colour, its first column transparent.
    anno::assets::Rgba parchment;
    parchment.width = 256;
    parchment.height = 133;
    parchment.pixels.assign(256 * 133 * 4, 0);
    for (std::size_t y = 0; y < 133; ++y)
        for (std::size_t x = 1; x < 256; ++x) {
            const std::size_t o = (y * 256 + x) * 4;
            parchment.pixels[o] = 10;
            parchment.pixels[o + 1] = 20;
            parchment.pixels[o + 2] = 30;
            parchment.pixels[o + 3] = 255;
        }
    const auto out =
        app::compose_minimap(cfg, rect, app::MinimapWindow{}, image, palette, &parchment);
    REQUIRE(out.width == 257);
    REQUIRE(out.height == 129);
    // The parchment lands one pixel right and down (quad_offset): raster (0, 0) is empty,
    // (1, 1) is its transparent first column, (2, 1) its colour (outside the diamond).
    CHECK(pixel(out, 0, 0)[3] == 0);
    CHECK(pixel(out, 1, 1)[3] == 0);
    CHECK(pixel(out, 2, 1) == std::array<std::uint8_t, 4>{10, 20, 30, 255});
    // The diamond's centre (128, 64), shifted by the quad offset to raster (129, 65), is cell
    // (2, 2): the palette's (9, 8).
    CHECK(pixel(out, 129, 65) == palette[9 * 16 + 8]);
    // Just below the top corner (128, 64 - 62.2 + 2, then + 1): u, v near 0 -> cell (0, 0), the
    // sea: transparent, so the parchment shows.
    CHECK(pixel(out, 129, 5) == std::array<std::uint8_t, 4>{10, 20, 30, 255});
}

TEST_CASE("minimap draw: the HUD draws the raster and the box under the chrome") {
    // A layout with no elements: only the minimap is drawn.
    const auto hud = app::parse_hud_layout("anno1503-ui-hud 1\nresolution 1024 768\n");
    REQUIRE(hud.ok());
    const app::ConstructionLayout construction;
    const app::MaskLayout masks;
    app::UiTexCache cache{fs::path{}};
    anno::assets::Rgba raster;
    raster.width = 2;
    raster.height = 1;
    raster.pixels = {1, 2, 3, 255, 0, 0, 0, 0};
    app::MinimapOverlay overlay;
    overlay.raster = &raster;
    overlay.x = 767;
    overlay.y = 10;
    overlay.box = {{900, 70}};
    anno::assets::Rgba canvas;
    const auto err = app::draw_live_screen_rgba_into(
        canvas, 1024, 768, hud.value(), construction, masks, app::UiMockSpec{}, cache, nullptr, {},
        false, false, false, false, nullptr, nullptr, &overlay);
    REQUIRE(err.empty());
    CHECK(pixel(canvas, 767, 10) == std::array<std::uint8_t, 4>{1, 2, 3, 255});
    CHECK(pixel(canvas, 768, 10)[3] == 0);
    CHECK(pixel(canvas, 900, 70) == std::array<std::uint8_t, 4>{255, 255, 255, 255});

    std::vector<std::uint32_t> xrgb(1024 * 768, 0x123456);
    const auto err2 = app::draw_live_screen_xrgb(
        xrgb.data(), 1024, 768, hud.value(), construction, masks, app::UiMockSpec{}, cache, nullptr,
        {}, false, false, false, false, nullptr, nullptr, &overlay);
    REQUIRE(err2.empty());
    CHECK(xrgb[10 * 1024 + 767] == 0x010203U);
    CHECK(xrgb[10 * 1024 + 768] == 0x123456U); // transparent: left as it was
    CHECK(xrgb[70 * 1024 + 900] == 0xffffffU);
}

// ---- UI-MINIMAP-1 step B2: owners, markers, the box's level factor ----

namespace {

/// The tutorial's players as the read gives them (docs/re/annoscene.md section 9: HEAD +0x4C of
/// 02Trade01.sze is 2 for player 0, 0 for player 1 (natives), 4 for player 2 (the human)).
app::MinimapOwnerRule tutorial_rule() {
    app::MinimapOwnerRule rule;
    rule.viewer = 2;
    rule.colour_of_player = {2U, 0U, 4U};
    return rule;
}

} // namespace

TEST_CASE("minimap owners: a city the viewer knows shows its owner's colour slot, others olive") {
    const app::MinimapConfig cfg;
    const app::MinimapOwnerRule rule = tutorial_rule();
    // AnnoWelt 0x100a404e..0x100a406c: known (bit of the viewer in city +0x61c) -> city +0x16, the
    // owner's colour slot; else 9 (0x100a4001).
    CHECK(app::minimap_city_owner(cfg, {0, 2, {2}}, rule) == 4);    // the human's own city: teal
    CHECK(app::minimap_city_owner(cfg, {1, 0, {0}}, rule) == 9);    // the computer's, unknown
    CHECK(app::minimap_city_owner(cfg, {2, 1, {1}}, rule) == 9);    // the natives', unknown
    CHECK(app::minimap_city_owner(cfg, {1, 0, {0, 2}}, rule) == 2); // known to the human: red
    // An owner without a colour slot: 0xff is stored as 0 (0x1002f437); one beyond the table too.
    app::MinimapOwnerRule no_slot = rule;
    no_slot.colour_of_player[2].reset();
    CHECK(app::minimap_city_owner(cfg, {0, 2, {2}}, no_slot) == 0);
    CHECK(app::minimap_city_owner(cfg, {0, 7, {2}}, rule) == 0);
    // World property 5 = 0: every city shows (the mask is all ones, 0x100a3f0d).
    app::MinimapConfig all = cfg;
    all.all_visible = true;
    CHECK(app::minimap_city_owner(all, {1, 0, {0}}, rule) == 2);
}

TEST_CASE("minimap owners: the published territory becomes the image's owner nibble") {
    app::MinimapConfig cfg;
    cfg.image_w = 8;
    cfg.image_h = 8; // one cell per tile of an 8 x 8 world (sx = sy = 1)
    // One island 4 x 2 at (2, 3), height 64 everywhere: a cell's shade is
    // ((sum of its 2 x 2 window / 4) + 8) >> 4.
    app::MinimapIsland heights;
    heights.x = 2;
    heights.y = 3;
    heights.w = 4;
    heights.h = 2;
    heights.heights.assign(8, 64);
    const app::MinimapWorld world = one_island(8, heights);
    // Tiles: row 0 city 0 (the human's); row 1 neutral (15), city 1 (the computer's), city 5 (no
    // such city), neutral.
    app::TerritoryView territory;
    territory.neutral_owner = 15;
    app::TerritoryIsland is;
    is.x0 = 2;
    is.y0 = 3;
    is.w = 4;
    is.h = 2;
    is.owner = {0, 0, 0, 0, 15, 1, 5, 15};
    is.cities = {{0, 2, {2}}, {1, 0, {0}}};
    territory.islands.push_back(is);
    const auto layers = app::minimap_owner_layers(cfg, territory, tutorial_rule());
    REQUIRE(layers.size() == 1);
    CHECK(layers[0].owners == std::vector<std::uint32_t>{4, 4, 4, 4, 9, 9, 9, 9});
    const auto image = app::build_minimap_image(cfg, world, &layers);
    REQUIRE(image.size() == 64);
    // Cell (2, 3): owner 4, window (2..3, 3..4) all island: 4 * 64 / 4 = 64 -> (64 + 8) >> 4 = 4.
    CHECK(cell(cfg, image, 2, 3) == 4 * 16 + 4);
    // Cell (5, 3): window (5..6, 3..4) half sea: 2 * 64 / 4 = 32 -> 2.
    CHECK(cell(cfg, image, 5, 3) == 4 * 16 + 2);
    // Cell (3, 4): the computer's tile, unknown to the viewer: olive; window (3..4, 4..5) has two
    // island tiles: 32 -> 2.
    CHECK(cell(cfg, image, 3, 4) == 9 * 16 + 2);
    // Without the layers the same cell is olive (B1).
    const auto plain = app::build_minimap_image(cfg, world);
    CHECK(cell(cfg, plain, 2, 3) == 9 * 16 + 4);
}

TEST_CASE("minimap markers: own ships dark green, soldiers green, flags white, others hidden") {
    app::MinimapConfig cfg;
    cfg.colour_depth = 16; // the GOG frame; at 32 the colours stay as they are (fix round case)
    const app::MinimapRect rect = app::minimap_rect(cfg, 1024); // (767, 10)
    const app::MinimapWindow whole{};
    // A 512-tile world. A ship at tile (256, 256): u = v = 64, the window's centre ->
    // (767 + 128, 10 + 64) = (895, 74). A soldier at (384, 256): u = 64 (du 0), v = 96
    // (dv 0.5): x = 895 - 88 * cos45 * 0.5 = 863.89, y = 74 + 44 * sin45 * 0.5 = 89.56 ->
    // (863, 89).
    const std::vector<app::MapFigure> figures = {
        {2, 384.0, 256.0, 0, 0},    // own soldier
        {2, 256.0, 256.0, 1, 0},    // own ship
        {0, 256.0, 256.0, 1, 0},    // the computer's ship
        {2, 384.0, 256.0, 0, 0x80}, // own soldier, flags & 0xc0 == 0x80: skipped
        {2, 384.0, 256.0, 0, 0xc0}, // flags & 0xc0 == 0xc0: drawn
        {2, 256.0, 256.0, 1, 0x10}, // own ship, flag 0x10: white
    };
    const auto m = app::minimap_markers(cfg, rect, whole, figures, 512, 512, 2);
    // The original's order: own list 1, own list 0. Colours as the RGB565 frame shows them:
    // 0x00d000 -> green 0xd0 >> 2 = 52, (52 * 255 + 31) / 63 = 210; 0x00ff00 and white stay.
    const std::vector<app::MinimapMarker> expected = {
        {895, 74, 0x00d200}, {895, 74, 0xffffff}, {863, 89, 0x00ff00}, {863, 89, 0x00ff00}};
    CHECK(m == expected);
    // The others' ships only when the world's id says so: 0xff3000 shows as (255, 49, 0)
    // (0x30 >> 2 = 12, (12 * 255 + 31) / 63 = 49).
    app::MinimapConfig others = cfg;
    others.marker_others_shown = true;
    const auto m2 = app::minimap_markers(others, rect, whole, figures, 512, 512, 2);
    REQUIRE(m2.size() == 5);
    CHECK(m2.back() == app::MinimapMarker{895, 74, 0xff3100});
    // Outside the window (zoom 0.15 around (0.1, 0.1)): nothing. At most marker_max.
    CHECK(app::minimap_markers(cfg, rect, app::minimap_window(0.15, 0.1, 0.1), figures, 512, 512, 2)
              .empty());
    app::MinimapConfig one = cfg;
    one.marker_max = 1;
    CHECK(app::minimap_markers(one, rect, whole, figures, 512, 512, 2).size() == 1);
    // The outline: the 8 pixels around the point, the centre left (the GOG shot's 3 x 3 ring).
    const auto px = app::minimap_marker_pixels(cfg, {{895, 74, 0x00d200}});
    REQUIRE(px.size() == 8);
    for (const auto& p : px) {
        CHECK(std::max(std::abs(p.x - 895), std::abs(p.y - 74)) == 1);
    }
}

TEST_CASE("minimap view box: the level factor is 2^level at the built levels") {
    const std::vector<double> levels = {84.0, 42.0, 20.0}; // docs/render/iso.md, groups 0..2
    CHECK(app::minimap_level_factor(levels, 84.0) == doctest::Approx(1.0));
    CHECK(app::minimap_level_factor(levels, 42.0) == doctest::Approx(2.0));
    // Group 2 is 4 (1 << 2, 0x1002ff5b), not 84 / 20 = 4.2.
    CHECK(app::minimap_level_factor(levels, 20.0) == doctest::Approx(4.0));
    // Between 42 and 20: 2^(1 + ln(42/30) / ln(42/20)) = 2^1.4535 = 2.739.
    CHECK(app::minimap_level_factor(levels, 30.0) == doctest::Approx(2.739).epsilon(0.001));
    CHECK(app::minimap_level_factor(levels, 168.0) == doctest::Approx(0.5));
    CHECK(app::minimap_level_factor(levels, 10.0) == doctest::Approx(8.0));
    CHECK(app::minimap_level_factor({}, 10.0) == doctest::Approx(1.0));
    // The box of the 512-tile tutorial world: e = level * 2200 / 512 = 4.297 map units at level
    // 0; its corners (minimap_view_box) make an axis-aligned rectangle 1.361 e across and 1.264 e
    // down (88 * cos45 / 64 = 0.972 px per unit across, half that down): 5.85 x 5.43 px at level
    // 0, 11.7 x 10.9 at level 1 (History Edition ui_5.mp4: 5.3 x 6.7 and 12.0 x 10.7 px, measured
    // at 0.75 scale, +-1.3 px; docs/re/ui.md 2.9 B2).
    const app::MinimapConfig cfg;
    const app::MinimapRect rect = app::minimap_rect(cfg, 1024);
    for (const double level : {1.0, 2.0}) {
        const auto box =
            app::minimap_view_box(cfg, rect, app::MinimapWindow{}, 256.0, 256.0, 512, 512, level);
        REQUIRE(box.size() == 4);
        double x0 = 1e9, x1 = -1e9, y0 = 1e9, y1 = -1e9;
        for (const auto& s : box) {
            x0 = std::min({x0, s.x0, s.x1});
            x1 = std::max({x1, s.x0, s.x1});
            y0 = std::min({y0, s.y0, s.y1});
            y1 = std::max({y1, s.y0, s.y1});
        }
        CHECK(x1 - x0 == doctest::Approx(level * 5.849).epsilon(0.002));
        CHECK(y1 - y0 == doctest::Approx(level * 5.431).epsilon(0.002));
    }
}

TEST_CASE("minimap draw: the markers are drawn over the box") {
    const auto hud = app::parse_hud_layout("anno1503-ui-hud 1\nresolution 1024 768\n");
    REQUIRE(hud.ok());
    const app::ConstructionLayout construction;
    const app::MaskLayout masks;
    app::UiTexCache cache{fs::path{}};
    app::MinimapOverlay overlay;
    overlay.box = {{900, 70}};
    overlay.marks = {{900, 70, 0x00d200}, {901, 70, 0x00d200}};
    anno::assets::Rgba canvas;
    REQUIRE(app::draw_live_screen_rgba_into(canvas, 1024, 768, hud.value(), construction, masks,
                                            app::UiMockSpec{}, cache, nullptr, {}, false, false,
                                            false, false, nullptr, nullptr, &overlay)
                .empty());
    CHECK(pixel(canvas, 900, 70) == std::array<std::uint8_t, 4>{0, 0xd2, 0, 255});
    std::vector<std::uint32_t> xrgb(1024 * 768, 0x123456);
    REQUIRE(app::draw_live_screen_xrgb(xrgb.data(), 1024, 768, hud.value(), construction, masks,
                                       app::UiMockSpec{}, cache, nullptr, {}, false, false, false,
                                       false, nullptr, nullptr, &overlay)
                .empty());
    CHECK(xrgb[70 * 1024 + 900] == 0x00d200U);
    CHECK(xrgb[70 * 1024 + 901] == 0x00d200U);
}

TEST_CASE("minimap config: the B2 keys") {
    const auto ok = app::parse_minimap_config(
        "anno1503-ui-minimap 1\nowner_rule 3 1\nmarker_colours 010203 040506 070809 0a0b0c\n"
        "marker_flags 20 30 10\nmarker_square 2 7\nmarker_others 1\nalerts 12\n");
    REQUIRE(ok.ok());
    CHECK(ok.value().no_colour == 3);
    CHECK(ok.value().all_visible);
    CHECK(ok.value().marker_list0 == 0x010203);
    CHECK(ok.value().marker_others == 0x0a0b0c);
    CHECK(ok.value().marker_flag == 0x20);
    CHECK(ok.value().marker_skip_value == 0x10);
    CHECK(ok.value().marker_half == 2);
    CHECK(ok.value().marker_max == 7);
    CHECK(ok.value().marker_others_shown);
    CHECK(ok.value().alert_max == 12);
    CHECK_FALSE(app::parse_minimap_config("anno1503-ui-minimap 1\nowner_rule 0 2\n").ok());
    CHECK_FALSE(app::parse_minimap_config("anno1503-ui-minimap 1\nmarker_square -1 5\n").ok());
}

TEST_CASE("minimap owners: a changed owner, city or mask is a different territory") {
    app::TerritoryView a;
    a.neutral_owner = 15;
    app::TerritoryIsland is;
    is.w = 2;
    is.h = 1;
    is.owner = {0, 15};
    is.cities = {{0, 2, {2}}};
    a.islands.push_back(is);
    app::TerritoryView b = a;
    b.version = 7; // the version is not content
    CHECK(a.same_content(b));
    b.islands[0].owner[1] = 0; // a claimed tile
    CHECK_FALSE(a.same_content(b));
    b = a;
    b.islands[0].cities[0].known_by.push_back(3); // another player learns of the city
    CHECK_FALSE(a.same_content(b));
    b = a;
    b.islands[0].cities[0].owner = 1; // a takeover
    CHECK_FALSE(a.same_content(b));
}

// UI-MINIMAP-1 C: the controls (docs/re/ui.md 2.9 Q6), values computed by hand.

TEST_CASE("minimap click: the hit test is the wrapper rectangle's inscribed diamond") {
    const app::MinimapConfig cfg;
    const auto rect = app::minimap_rect(cfg, 1024); // drawn at (767, 10) 256 x 128
    // Relative to the wrapper's (765, 12) = (767 - 2, 10 + 2) (AnnoFrame 0x10002970): the centre
    // (893, 76) is (128, 64): t = 64, dx = 0.
    CHECK(app::minimap_hit(cfg, rect, 893, 76));
    // The left corner: xr = 0, dx = -128 >= -2 * 64; one pixel further left is outside the
    // rectangle. It lies left of the chrome (x < 779).
    CHECK(app::minimap_hit(cfg, rect, 765, 76));
    CHECK_FALSE(app::minimap_hit(cfg, rect, 764, 76));
    CHECK(app::minimap_hit(cfg, rect, 770, 76));
    // The right corner: xr = 255, dx = 127 < 128; xr = 256 is outside.
    CHECK(app::minimap_hit(cfg, rect, 1020, 76));
    CHECK_FALSE(app::minimap_hit(cfg, rect, 1021, 76));
    // The top: yr = 0 gives t = 0 and no dx with -0 <= dx < 0; yr = 1 gives [-2, 2).
    CHECK_FALSE(app::minimap_hit(cfg, rect, 893, 12));
    CHECK(app::minimap_hit(cfg, rect, 893, 13));
    CHECK(app::minimap_hit(cfg, rect, 894, 13));
    CHECK_FALSE(app::minimap_hit(cfg, rect, 895, 13)); // dx = 2, not < 2
    // The bottom: yr = 127 is past h / 2, t = 128 - 127 = 1; yr = 128 is outside.
    CHECK(app::minimap_hit(cfg, rect, 893, 139));
    CHECK_FALSE(app::minimap_hit(cfg, rect, 893, 140));
    // The rectangle's corner outside the diamond: (5, 3), dx = -123 < -6.
    CHECK_FALSE(app::minimap_hit(cfg, rect, 770, 15));
    // Right-anchored in a wider layer: the same point 1518 px further right.
    const auto wide = app::minimap_rect(cfg, 2542);
    CHECK(app::minimap_hit(cfg, wide, 893 + 1518, 76));
    CHECK_FALSE(app::minimap_hit(cfg, wide, 893, 76));
}

TEST_CASE("minimap click: screen to world at turn 0, the whole world and a zoom window") {
    const app::MinimapConfig cfg;
    const auto rect = app::minimap_rect(cfg, 1024);
    const app::MinimapWindow whole;
    // (900, 50): (xr, yr) = (135, 38); X = (135 - 6) * 1.03 = 132.87, Y = 38 * 1.03 = 39.14,
    // D = X - 128 = 4.87; P = 256 / 128 * (Y + D / 2) = 83.15, Q = 2 * (Y - D / 2) = 73.41;
    // x0 = trunc(512 * 73.41 / 256) = 146, y0 = trunc(512 * 83.15 / 256) = 166; the whole
    // window leaves them.
    using Tile = std::optional<std::pair<std::int64_t, std::int64_t>>;
    const auto tile = [](std::int64_t x, std::int64_t y) { return Tile(std::pair{x, y}); };
    CHECK(app::minimap_world_at(cfg, rect, whole, 512, 512, 900, 50) == tile(146, 166));
    // A 512 x 256 world: y0 = trunc(256 * 83.15 / 256) = 83 (y follows P and the height).
    CHECK(app::minimap_world_at(cfg, rect, whole, 512, 256, 900, 50) == tile(146, 83));
    // z = 0.4 around (u, v) = (0.3, 0.7): u in [0.1, 0.5], v in [0.5, 0.9];
    // wx = trunc(((146 / 512 - 0.5) * 0.4 + 0.7) * 512) = trunc(314.4) = 314,
    // wy = trunc(((166 / 512 - 0.5) * 0.4 + 0.3) * 512) = trunc(117.6) = 117.
    const auto zoomed = app::minimap_window(0.4, 0.3, 0.7);
    CHECK(app::minimap_world_at(cfg, rect, zoomed, 512, 512, 900, 50) == tile(314, 117));
    // Pushed back at the edges: (0.1, 0.95) -> u [0, 0.4], v [0.6, 1.0], mids 0.2 and 0.8:
    // wx = trunc((-0.0859375 + 0.8) * 512) = 365, wy = trunc((-0.0703125 + 0.2) * 512) = 66.
    const auto edge = app::minimap_window(0.4, 0.1, 0.95);
    CHECK(app::minimap_world_at(cfg, rect, edge, 512, 512, 900, 50) == tile(365, 66));
    // Outside the diamond, or no world: nothing.
    CHECK_FALSE(app::minimap_world_at(cfg, rect, whole, 512, 512, 770, 15).has_value());
    CHECK_FALSE(app::minimap_world_at(cfg, rect, whole, 0, 0, 900, 50).has_value());
    // The inverse undoes the drawing transform (minimap_screen) to within a few tiles (a pixel is
    // about 2 tiles of a 512-tile world; the 1.03 and -6 make up for the 124.5 px half-diagonal).
    for (const auto& [wx, wy] : {std::pair{300, 200}, std::pair{60, 450}, std::pair{256, 256}}) {
        CAPTURE(wx);
        CAPTURE(wy);
        const auto [sx, sy] =
            app::minimap_screen(cfg, rect, whole, 128.0 * wy / 512.0, 128.0 * wx / 512.0);
        const auto back =
            app::minimap_world_at(cfg, rect, whole, 512, 512, static_cast<int>(std::lround(sx)),
                                  static_cast<int>(std::lround(sy)));
        REQUIRE(back.has_value());
        CHECK(std::abs(back->first - wx) <= 4);
        CHECK(std::abs(back->second - wy) <= 4);
    }
}

TEST_CASE("minimap click: the click area takes the parchment left of the chrome") {
    const app::MinimapConfig cfg;
    // The drawn raster (767, 10) 258 x 130 (the parchment quad, fix F2) and the wrapper's
    // (765, 12) 256 x 128 together: x 765..1024, y 10..139.
    CHECK(app::minimap_click_area(cfg, 1024) == app::MinimapRect{765, 10, 260, 130, 1.0});
    CHECK(app::minimap_click_area(cfg, 2542) == app::MinimapRect{2283, 10, 260, 130, 1.0});
}

TEST_CASE("minimap click: the HUD hit test takes the click area, also left of the column") {
    const auto hud = app::load_hud_layout(fs::path(ANNO_REPO_DIR) / "data" / "ui" / "hud.txt");
    REQUIRE(hud.ok());
    const app::MinimapConfig cfg;
    const auto rect = [&](int ui_w) {
        const app::MinimapRect a = app::minimap_click_area(cfg, ui_w);
        return app::UiRect{a.x, a.y, a.w, a.h};
    };
    const auto at = app::place_hud(hud.value(), 1024, 768);
    // (770, 30) is parchment left of the column (x 779): the map without the minimap, chrome
    // with it, its reference point itself.
    CHECK_FALSE(app::ui_hit(hud.value(), at, 770, 30).chrome);
    const auto hit = app::ui_hit(hud.value(), at, 770, 30, false, {}, rect(1024));
    CHECK(hit.chrome);
    CHECK(hit.ref == std::optional<std::pair<int, int>>({770, 30}));
    // Outside the area (x 765..1023, y 10..139): the map.
    CHECK_FALSE(app::ui_hit(hud.value(), at, 764, 30, false, {}, rect(1024)).chrome);
    CHECK_FALSE(app::ui_hit(hud.value(), at, 770, 140, false, {}, rect(1024)).chrome);
    // Right-anchored at 2542 x 1351: 1518 px further right, the same reference point.
    const auto big = app::place_hud(hud.value(), 2542, 1351);
    const auto wide = app::ui_hit(hud.value(), big, 770 + 1518, 30, false, {}, rect(2542));
    CHECK(wide.chrome);
    CHECK(wide.ref == std::optional<std::pair<int, int>>({770, 30}));
    // The magnifier's buttons are hud.txt elements (measured on hud_idle_running).
    const auto* plus = hud.value().element("lupe_plus");
    const auto* minus = hud.value().element("lupe_minus");
    REQUIRE(plus != nullptr);
    REQUIRE(minus != nullptr);
    CHECK(std::pair{plus->pos_x, plus->pos_y} == std::pair{796, 135});
    CHECK(std::pair{minus->pos_x, minus->pos_y} == std::pair{826, 153});
}

TEST_CASE("minimap zoom: the top-right + and - step by 0.15 inside 0.15 .. 1.0") {
    const app::MinimapConfig cfg;
    // From the whole world, + six times: 0.85, 0.7, 0.55, 0.4, 0.25, then 0.1 is clamped to 0.15
    // (0x10033770 -> 0x10033710; float steps).
    double z = cfg.zoom_start;
    const std::array<double, 7> closer = {0.85, 0.7, 0.55, 0.4, 0.25, 0.15, 0.15};
    for (const double want : closer) {
        z = app::minimap_zoom_step(cfg, z, true);
        CHECK(z == doctest::Approx(want).epsilon(1e-6));
    }
    CHECK(z == static_cast<double>(0.15F));
    // And back: 0.3, 0.45, 0.6, 0.75, 0.9, then 1.05 is clamped to 1.0.
    const std::array<double, 7> farther = {0.3, 0.45, 0.6, 0.75, 0.9, 1.0, 1.0};
    for (const double want : farther) {
        z = app::minimap_zoom_step(cfg, z, false);
        CHECK(z == doctest::Approx(want).epsilon(1e-6));
    }
    CHECK(z == 1.0);
    // The range, the step and the click numbers are data.
    const auto custom = app::parse_minimap_config(
        "anno1503-ui-minimap 1\nzoom_range 0.05 1.0 0.5\nclick -5 2 1.5\n");
    REQUIRE(custom.ok());
    CHECK(app::minimap_zoom_step(custom.value(), 1.0, true) == 0.5);
    CHECK(app::minimap_zoom_step(custom.value(), 0.5, true) == static_cast<double>(0.05F));
    CHECK(custom.value().click_dx == -5);
    CHECK(custom.value().click_dy == 2);
    CHECK(custom.value().click_scale == 1.5);
    CHECK_FALSE(app::parse_minimap_config("anno1503-ui-minimap 1\nzoom_range 0.5 0.2 0.1\n").ok());
    CHECK_FALSE(app::parse_minimap_config("anno1503-ui-minimap 1\nzoom_range 0 1 0.1\n").ok());
    CHECK_FALSE(app::parse_minimap_config("anno1503-ui-minimap 1\nclick -6 0 0\n").ok());
}

TEST_CASE("minimap magnifier: one press is one built level, clamped at both ends") {
    // The original's levels 0..2 (commands 0xa and 9, AnnoFrame 0x10007710 / 0x10007692) at the
    // rebuild's tile widths 84, 42, 20.
    const std::vector<double> levels = {84.0, 42.0, 20.0};
    using W = std::optional<double>;
    CHECK(app::magnifier_target(levels, 20.0, true) == W(42.0));
    CHECK(app::magnifier_target(levels, 42.0, true) == W(84.0));
    CHECK(app::magnifier_target(levels, 84.0, true) == std::nullopt); // not below level 0
    CHECK(app::magnifier_target(levels, 84.0, false) == W(42.0));
    CHECK(app::magnifier_target(levels, 42.0, false) == W(20.0));
    CHECK(app::magnifier_target(levels, 20.0, false) == std::nullopt); // not above level 2
    // The continuous zoom between two levels: to the next level in that direction.
    CHECK(app::magnifier_target(levels, 30.0, true) == W(42.0));
    CHECK(app::magnifier_target(levels, 30.0, false) == W(20.0));
    // Beyond the levels (the continuous zoom's extension): back to the nearest level.
    CHECK(app::magnifier_target(levels, 100.0, true) == std::nullopt);
    CHECK(app::magnifier_target(levels, 100.0, false) == W(84.0));
    CHECK(app::magnifier_target(levels, 10.0, true) == W(20.0));
    CHECK(app::magnifier_target({}, 20.0, true) == std::nullopt);
}

// ---- UI-MINIMAP-2: the continuous zoom's magnifier reaches the wheel's furthest zoom-out ----

TEST_CASE("minimap magnifier stages: halvings past level 2, then the wheel's end itself") {
    // Owner decision 2026-11-17 (ui_6.mp4): the magnifier's - goes as far as the wheel.
    const std::vector<double> built = {84.0, 42.0, 20.0};
    using V = std::vector<double>;
    using W = std::optional<double>;
    // Several halvings, then the exact minimum (0.625, the next halving, is below 1.1).
    CHECK(app::magnifier_stages(built, 1.1) == V{84.0, 42.0, 20.0, 10.0, 5.0, 2.5, 1.25, 1.1});
    CHECK(app::magnifier_stages(built, 1.5) == V{84.0, 42.0, 20.0, 10.0, 5.0, 2.5, 1.5});
    // A minimum just below a halving: the short last hop is still a stage.
    CHECK(app::magnifier_stages(built, 4.9) == V{84.0, 42.0, 20.0, 10.0, 5.0, 4.9});
    // A minimum on a halving: that width once, as the last stage.
    CHECK(app::magnifier_stages(built, 5.0) == V{84.0, 42.0, 20.0, 10.0, 5.0});
    // A minimum between level 2 and its first halving: one extra stage.
    CHECK(app::magnifier_stages(built, 14.142) == V{84.0, 42.0, 20.0, 14.142});
    // A minimum at or above level 2, or none: no extra stages (a small scene, a large window).
    CHECK(app::magnifier_stages(built, 20.0) == built);
    CHECK(app::magnifier_stages(built, 25.0) == built);
    CHECK(app::magnifier_stages(built, 0.0) == built);
    CHECK(app::magnifier_stages(built, -1.0) == built);
    CHECK(app::magnifier_stages({}, 1.0).empty());
    // The levels are data: one built level of 32 halves to 16 and 8, then the end 7.
    CHECK(app::magnifier_stages({32.0}, 7.0) == V{32.0, 16.0, 8.0, 7.0});

    // Minus from level 2 walks every stage and stops on the minimum; plus walks back to level 0.
    const auto stages = app::magnifier_stages(built, 1.1);
    double z = 20.0;
    for (const double want : {10.0, 5.0, 2.5, 1.25, 1.1}) {
        const auto next = app::magnifier_target(stages, z, false);
        REQUIRE(next.has_value());
        CHECK(*next == want);
        z = *next;
    }
    CHECK(app::magnifier_target(stages, z, false) == std::nullopt); // the wheel's end
    for (const double want : {1.25, 2.5, 5.0, 10.0, 20.0, 42.0, 84.0}) {
        const auto next = app::magnifier_target(stages, z, true);
        REQUIRE(next.has_value());
        CHECK(*next == want);
        z = *next;
    }
    CHECK(app::magnifier_target(stages, z, true) == std::nullopt); // level 0

    // A view the wheel left between two stages goes to the next stage in the pressed direction.
    CHECK(app::magnifier_target(stages, 7.0, false) == W(5.0));
    CHECK(app::magnifier_target(stages, 7.0, true) == W(10.0));
    CHECK(app::magnifier_target(stages, 1.2, false) == W(1.1));
    CHECK(app::magnifier_target(stages, 1.2, true) == W(1.25));
    CHECK(app::magnifier_target(stages, 30.0, false) == W(20.0));
    // Without extra stages the clamp at level 2 stays.
    CHECK(app::magnifier_target(app::magnifier_stages(built, 20.0), 20.0, false) == std::nullopt);
}

TEST_CASE("minimap magnifier stages: doublings past level 0, then the wheel's closest zoom-in") {
    // Fix round R2 (owner, ui_7.mp4): the magnifier's + goes as close as the wheel.
    const std::vector<double> built = {84.0, 42.0, 20.0};
    using V = std::vector<double>;
    using W = std::optional<double>;
    // The wheel's ceiling at the original levels: 84 * sqrt 2 = 118.79, below the first doubling
    // (168), so the ceiling alone is the one extra stage.
    CHECK(app::magnifier_stages(built, 20.0, 118.8) == V{118.8, 84.0, 42.0, 20.0});
    // Several doublings, widest first, then the built levels.
    CHECK(app::magnifier_stages(built, 20.0, 400.0) == V{400.0, 336.0, 168.0, 84.0, 42.0, 20.0});
    // A ceiling just above a doubling: the short last hop is still a stage.
    CHECK(app::magnifier_stages(built, 20.0, 170.0) == V{170.0, 168.0, 84.0, 42.0, 20.0});
    // A ceiling on a doubling: that width once.
    CHECK(app::magnifier_stages(built, 20.0, 168.0) == V{168.0, 84.0, 42.0, 20.0});
    // A ceiling at or below level 0, or none: no extra stages on that side.
    CHECK(app::magnifier_stages(built, 20.0, 84.0) == built);
    CHECK(app::magnifier_stages(built, 20.0, 60.0) == built);
    CHECK(app::magnifier_stages(built, 20.0, 0.0) == built);
    CHECK(app::magnifier_stages(built, 20.0, -1.0) == built);
    CHECK(app::magnifier_stages(built, 20.0) == built);
    CHECK(app::magnifier_stages({}, 1.0, 100.0).empty());
    // Both sides at once.
    const auto stages = app::magnifier_stages(built, 1.1, 118.8);
    CHECK(stages == V{118.8, 84.0, 42.0, 20.0, 10.0, 5.0, 2.5, 1.25, 1.1});

    // Plus from level 2 walks to the ceiling and stops there; minus walks back to the far end.
    double z = 20.0;
    for (const double want : {42.0, 84.0, 118.8}) {
        const auto next = app::magnifier_target(stages, z, true);
        REQUIRE(next.has_value());
        CHECK(*next == want);
        z = *next;
    }
    CHECK(app::magnifier_target(stages, z, true) == std::nullopt); // the wheel's closest zoom-in
    for (const double want : {84.0, 42.0, 20.0, 10.0, 5.0, 2.5, 1.25, 1.1}) {
        const auto next = app::magnifier_target(stages, z, false);
        REQUIRE(next.has_value());
        CHECK(*next == want);
        z = *next;
    }
    CHECK(app::magnifier_target(stages, z, false) == std::nullopt);

    // A view the wheel left between level 0 and the ceiling: the next stage in that direction.
    CHECK(app::magnifier_target(stages, 100.0, true) == W(118.8));
    CHECK(app::magnifier_target(stages, 100.0, false) == W(84.0));
    // Without the ceiling the clamp at level 0 stays.
    CHECK(app::magnifier_target(app::magnifier_stages(built, 1.1), 84.0, true) == std::nullopt);
}

// ---- UI-MINIMAP-1 fix round: F1, the scroll's rolled left end ----

TEST_CASE("minimap scroll: the rolled left end is chrome left of the column at any width") {
    const auto hud = app::load_hud_layout(fs::path(ANNO_REPO_DIR) / "data" / "ui" / "hud.txt");
    REQUIRE(hud.ok());
    // fenster.tex id 3, measured at (745, 49) on hud_idle_running and the History Edition's
    // lossless capture (hud.txt), right-anchored like the column.
    const auto* curl = hud.value().element("minimap_curl");
    REQUIRE(curl != nullptr);
    CHECK(curl->tex_id == std::optional<int>(3));
    CHECK(std::pair{curl->pos_x, curl->pos_y} == std::pair{745, 49});
    CHECK(std::pair{curl->size_w, curl->size_h} == std::pair{34U, 47U});
    for (const int w : {1024, 2560}) {
        const int dx = w - 1024;
        const auto at = app::place_hud(hud.value(), w, 768);
        // (750, 60): on the curl, left of the column and of the minimap's click area.
        const auto hit = app::ui_hit(hud.value(), at, 750 + dx, 60);
        CHECK(hit.chrome);
        CHECK(hit.ref == std::optional<std::pair<int, int>>({750, 60}));
        CHECK(app::ui_hit(hud.value(), at, 745 + dx, 49).chrome);
        CHECK(app::ui_hit(hud.value(), at, 778 + dx, 95).chrome);
        // Just outside its rectangle: the map.
        CHECK_FALSE(app::ui_hit(hud.value(), at, 744 + dx, 60).chrome);
        CHECK_FALSE(app::ui_hit(hud.value(), at, 750 + dx, 48).chrome);
        CHECK_FALSE(app::ui_hit(hud.value(), at, 750 + dx, 96).chrome);
    }
}

TEST_CASE("minimap scroll: both UI paths draw the rolled left end left of the column") {
    const auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("skipping: ANNO1503_GAME_DIR is not set");
        return;
    }
    const auto hud = app::load_hud_layout(fs::path(ANNO_REPO_DIR) / "data" / "ui" / "hud.txt");
    REQUIRE(hud.ok());
    const app::ConstructionLayout construction;
    const app::MaskLayout masks;
    app::UiTexCache cache(*game);
    const anno::assets::Rgba* sprite = cache.decode("toolgfx/fenster.tex", 3);
    REQUIRE(sprite != nullptr);
    REQUIRE(sprite->width == 34);
    REQUIRE(sprite->height == 47);
    // At 1024 x 768 and at a maximised 2560 x 1369 window: every opaque pixel of id 3 lands at
    // (745 + W - 1024, 49) + its own position, in the GPU path's RGBA layer and in the CPU
    // path's XRGB frame alike; without the curl these are map pixels (transparent / untouched).
    for (const auto& [w, h] : {std::pair{1024, 768}, std::pair{2560, 1369}}) {
        const int x0 = 745 + w - 1024, y0 = 49;
        anno::assets::Rgba canvas;
        const auto err = app::draw_live_screen_rgba_into(
            canvas, w, h, hud.value(), construction, masks, app::UiMockSpec{}, cache, nullptr, {},
            false, false, false, false, nullptr, nullptr, nullptr);
        REQUIRE_MESSAGE(err.empty(), err);
        std::vector<std::uint32_t> xrgb(static_cast<std::size_t>(w) * static_cast<std::size_t>(h),
                                        0x123456);
        const auto err2 = app::draw_live_screen_xrgb(
            xrgb.data(), w, h, hud.value(), construction, masks, app::UiMockSpec{}, cache, nullptr,
            {}, false, false, false, false, nullptr, nullptr, nullptr);
        REQUIRE_MESSAGE(err2.empty(), err2);
        std::size_t opaque = 0, rgba_same = 0, xrgb_same = 0;
        for (int sy = 0; sy < sprite->height; ++sy) {
            for (int sx = 0; sx < sprite->width; ++sx) {
                const auto s = pixel(*sprite, sx, sy);
                if (s[3] == 0)
                    continue;
                ++opaque;
                const auto c = pixel(canvas, x0 + sx, y0 + sy);
                rgba_same += c == std::array<std::uint8_t, 4>{s[0], s[1], s[2], 255} ? 1U : 0U;
                const std::uint32_t want =
                    (std::uint32_t{s[0]} << 16) | (std::uint32_t{s[1]} << 8) | std::uint32_t{s[2]};
                xrgb_same += xrgb[static_cast<std::size_t>(y0 + sy) * static_cast<std::size_t>(w) +
                                  static_cast<std::size_t>(x0 + sx)] == want
                                 ? 1U
                                 : 0U;
            }
        }
        INFO("window " << w << " x " << h);
        CHECK(opaque == 1022); // fenster.tex id 3's opaque count (anno-dump tex)
        CHECK(rgba_same == opaque);
        CHECK(xrgb_same == opaque);
        // The column itself starts at 779 + W - 1024: the curl is the part left of it.
        CHECK(x0 + sprite->width == 779 + w - 1024);
    }
}

// ---- UI-MINIMAP-1 fix round: F2, the parchment and the colours as the History Edition draws them

TEST_CASE("minimap config: the History Edition's parchment quad and colour depth from data") {
    const auto loaded =
        app::load_minimap_config(fs::path(ANNO_REPO_DIR) / "data" / "ui" / "minimap.txt");
    REQUIRE(loaded.ok());
    // The code's strip W' = x1 - x0 + 2, H' = y1 - y0 + 2 (AnnoFrame 0x10031ef1..0x10031f1d), as
    // the History Edition draws it, and its 32-bit colours.
    CHECK(loaded.value().parchment_x0 == 0);
    CHECK(loaded.value().parchment_y0 == 0);
    CHECK(loaded.value().parchment_x1 == 2);
    CHECK(loaded.value().parchment_y1 == 2);
    CHECK(loaded.value().colour_depth == 32);
    // The GOG variant is two lines.
    const auto gog = app::parse_minimap_config(
        "anno1503-ui-minimap 1\nparchment_quad 1 1 1 1\ncolour_depth 16\n");
    REQUIRE(gog.ok());
    CHECK(gog.value().parchment_x0 == 1);
    CHECK(gog.value().parchment_y1 == 1);
    CHECK(gog.value().colour_depth == 16);
    CHECK_FALSE(app::parse_minimap_config("anno1503-ui-minimap 1\ncolour_depth 24\n").ok());
    CHECK_FALSE(app::parse_minimap_config("anno1503-ui-minimap 1\nparchment_quad -1 0 2 2\n").ok());
    CHECK_FALSE(app::parse_minimap_config("anno1503-ui-minimap 1\nparchment_quad 0 0 2\n").ok());
}

TEST_CASE("minimap palette: the History Edition's 32-bit entries") {
    const app::MinimapConfig cfg; // colour_depth 32
    const auto pal = app::minimap_palette(cfg);
    REQUIRE(pal.size() == 160);
    CHECK(pal[9 * 16 + 1][3] == 0);
    // floor(channel * (k + 16) / 31): (9, 3) b2ab00 * 19 / 31 = (109.1, 104.8, 0) -> (109, 104,
    // 0), the History Edition capture's commonest land colour; (9, 4) * 20 / 31 = (114.8, 110.3,
    // 0); (1, 3) 00bbff * 19 / 31 = (0, 114.6, 156.3): all three are on its lossless capture.
    CHECK(pal[9 * 16 + 3] == std::array<std::uint8_t, 4>{109, 104, 0, 255});
    CHECK(pal[9 * 16 + 4] == std::array<std::uint8_t, 4>{114, 110, 0, 255});
    CHECK(pal[1 * 16 + 3] == std::array<std::uint8_t, 4>{0, 114, 156, 255});
    // (9, 2) 664400 * 18 / 31 = (59.2, 39.5, 0); (8, 15) fafafa * 31 / 31.
    CHECK(pal[9 * 16 + 2] == std::array<std::uint8_t, 4>{59, 39, 0, 255});
    CHECK(pal[8 * 16 + 15] == std::array<std::uint8_t, 4>{250, 250, 250, 255});
    // The markers keep their colours at 32 bits (0x00d000, not the RGB565 frame's 0x00d200).
    const app::MinimapRect rect = app::minimap_rect(cfg, 1024);
    const auto m = app::minimap_markers(cfg, rect, app::MinimapWindow{}, {{2, 256.0, 256.0, 1, 0}},
                                        512, 512, 2);
    REQUIRE(m.size() == 1);
    CHECK(m[0] == app::MinimapMarker{895, 74, 0x00d000});
}

TEST_CASE("minimap compose: the parchment quad is sampled bilinearly over 258 x 130") {
    app::MinimapConfig cfg;
    cfg.image_w = 4;
    cfg.image_h = 4;
    const auto rect = app::minimap_rect(cfg, 1024); // (767, 10) 256 x 128
    const auto palette = app::minimap_palette(cfg);
    const std::vector<std::uint16_t> image(16, 0); // all sea: transparent, the parchment shows
    // A ramp: red = column, green = row, opaque.
    anno::assets::Rgba parchment;
    parchment.width = 256;
    parchment.height = 133;
    parchment.pixels.assign(256 * 133 * 4, 255);
    for (std::size_t y = 0; y < 133; ++y)
        for (std::size_t x = 0; x < 256; ++x) {
            const std::size_t o = (y * 256 + x) * 4;
            parchment.pixels[o] = static_cast<std::uint8_t>(x);
            parchment.pixels[o + 1] = static_cast<std::uint8_t>(y);
            parchment.pixels[o + 2] = 0;
        }
    const auto he =
        app::compose_minimap(cfg, rect, app::MinimapWindow{}, image, palette, &parchment);
    REQUIRE(he.width == 258);
    REQUIRE(he.height == 130);
    // Pixel (x, y) of the quad reads texel ((x + 0.5) * 256 / 258 - 0.5, (y + 0.5) * 133 / 130 -
    // 0.5). x = 200: u = 198.446 -> red 198; y = 65: v = 66.512 -> green 67. x = 0: u = -0.004,
    // clamped to column 0. x = 257 (the 258th column, past the GOG fit): u = 255.0 -> 255.
    CHECK(pixel(he, 200, 65) == std::array<std::uint8_t, 4>{198, 67, 0, 255});
    CHECK(pixel(he, 0, 0) == std::array<std::uint8_t, 4>{0, 0, 0, 255});
    CHECK(pixel(he, 257, 129) == std::array<std::uint8_t, 4>{255, 132, 0, 255});
    // The GOG frame's fit (parchment_quad 1 1 1 1): the columns land on texel centres, so raster
    // x 201 is column 200 exactly and raster y 66 is v = 65.5 * 133 / 128 - 0.5 = 67.56 -> 68;
    // with colour_depth 16 the pixel is then shown in RGB565: red 200 >> 3 = 25 -> 206, green
    // 68 >> 2 = 17 -> 69.
    app::MinimapConfig gog = cfg;
    gog.parchment_x0 = gog.parchment_y0 = gog.parchment_x1 = gog.parchment_y1 = 1;
    const auto g32 =
        app::compose_minimap(gog, rect, app::MinimapWindow{}, image, palette, &parchment);
    REQUIRE(g32.width == 257);
    CHECK(pixel(g32, 201, 66) == std::array<std::uint8_t, 4>{200, 68, 0, 255});
    CHECK(pixel(g32, 0, 0)[3] == 0); // the quad starts one pixel in
    gog.colour_depth = 16;
    const auto g16 =
        app::compose_minimap(gog, rect, app::MinimapWindow{}, image, palette, &parchment);
    CHECK(pixel(g16, 201, 66) == std::array<std::uint8_t, 4>{206, 69, 0, 255});
    // The grain: a one-pixel checkerboard keeps its full step in the GOG fit's columns (each
    // pixel one texel) and loses it where the 258-pixel quad samples between two texels.
    for (std::size_t y = 0; y < 133; ++y)
        for (std::size_t x = 0; x < 256; ++x) {
            const std::size_t o = (y * 256 + x) * 4;
            const std::uint8_t v = ((x + y) % 2) != 0 ? 200 : 0;
            parchment.pixels[o] = parchment.pixels[o + 1] = parchment.pixels[o + 2] = v;
        }
    const auto he_c =
        app::compose_minimap(cfg, rect, app::MinimapWindow{}, image, palette, &parchment);
    gog.colour_depth = 32;
    const auto gog_c =
        app::compose_minimap(gog, rect, app::MinimapWindow{}, image, palette, &parchment);
    // The mean step between horizontal neighbours over one row, where the row filter keeps most
    // of the pattern: raster row 1 of the GOG fit (v = 0.0, texel row 0 alone) and row 0 of the
    // quad (v = 0.012).
    const auto step = [](const anno::assets::Rgba& im, int x0, int x1, int y) {
        double sum = 0.0;
        for (int x = x0; x < x1; ++x)
            sum += std::abs(static_cast<double>(pixel(im, x + 1, y)[0]) - pixel(im, x, y)[0]);
        return sum / (x1 - x0);
    };
    const double gog_step = step(gog_c, 20, 236, 1);
    const double he_step = step(he_c, 20, 236, 0);
    INFO("GOG fit " << gog_step << ", History Edition " << he_step);
    CHECK(gog_step > 190.0);
    CHECK(he_step < 0.75 * gog_step);
}
