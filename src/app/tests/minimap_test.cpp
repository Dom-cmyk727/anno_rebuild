// UI-MINIMAP-1 step B1: the minimap's data, image, palette, geometry and drawing (minimap.hpp),
// against values computed by hand from docs/re/ui.md section 2.9's formulas (each case shows the
// arithmetic) and the shipped data/ui/minimap.txt. No original files needed.
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/minimap.hpp>
#include <anno/app/ui_layout.hpp>
#include <anno/app/ui_mock.hpp>

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
    const app::MinimapConfig cfg;
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

TEST_CASE("minimap image: owner * 16 + shade from a synthetic island") {
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
