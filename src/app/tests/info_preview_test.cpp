// UI-INFO-2b: the info panel picture's entrance arrows and its one canvas over four turns
// (anno::app::info_preview; docs/re/ui.md section 2.5). Synthetic inputs only: no original
// bytes, so these cases run everywhere. The GOG pixel check is
// tools/test/info_panel_gog_compare.py (f12_info_stall_1310 / 1314 exact).
#include <cstdint>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/info_preview.hpp>

namespace ip = anno::app::info_preview;

namespace {

anno::assets::Rgba solid(std::uint16_t w, std::uint16_t h, std::uint8_t r) {
    anno::assets::Rgba img;
    img.width = w;
    img.height = h;
    img.pixels.resize(static_cast<std::size_t>(w) * h * 4);
    for (std::size_t i = 0; i < img.pixels.size(); i += 4) {
        img.pixels[i] = r;
        img.pixels[i + 3] = 255;
    }
    return img;
}

std::uint8_t red_at(const anno::assets::Rgba& img, int x, int y) {
    return img.pixels[(static_cast<std::size_t>(y) * img.width + static_cast<std::size_t>(x)) * 4];
}

} // namespace

TEST_CASE("info preview: the drawn arrows of a decoded OBJECTPOS record") {
    // CONPO2 with two arrows of which the second has side 5 (skipped), and a CONPOS (not drawn).
    anno::formats::objectpos::ObjectPos o;
    anno::formats::objectpos::SubRecord fire{{'F', 'I', 'R', 'E', 0, 0}, {}};
    fire.data.assign(16, 0);
    anno::formats::objectpos::SubRecord conpo2{{'C', 'O', 'N', 'P', 'O', '2'}, {}};
    conpo2.data = {0, 2, 1, 4, 4, 5};
    anno::formats::objectpos::SubRecord conpos{{'C', 'O', 'N', 'P', 'O', 'S'}, {}};
    conpos.data.push_back(0);
    conpos.data.push_back(4);
    o.records = {fire, conpo2, conpos};
    const auto arrows = ip::entrance_arrows(o);
    REQUIRE(arrows.size() == 1);
    CHECK(arrows[0] == ip::EntranceArrow{0, 2, 1});
    o.records = {conpos};
    CHECK(ip::entrance_arrows(o).empty());
}

TEST_CASE("info preview: the projection truncates toward zero") {
    // 59.1633 cos 45 = 41.835 px per tile of (y - x); 41.835 sin(42 pi / 256) = 20.621 per (x + y).
    CHECK(ip::project(0.0, 1.0) == std::pair{41, 20});
    // Toward zero, not floor: -41.835 is -41 (floor would be -42).
    CHECK(ip::project(1.0, 0.0) == std::pair{-41, 20});
    CHECK(ip::project(0.5, 0.5) == std::pair{0, 20});
    CHECK(ip::project(-0.334, 0.167) == std::pair{20, -3});
}

TEST_CASE("info preview: an arrow turns with the footprint") {
    // Side 1 (+y) at third (0, 2) of a 1 x 1 footprint: u = 0.167, v = 0.667 + 0.167 + 0.5.
    const ip::EntranceArrow a{0, 2, 1};
    const auto p0 = ip::place_arrow(a, 1, 1, 0);
    CHECK(p0.x == doctest::Approx(0.167).epsilon(1e-6));
    CHECK(p0.y == doctest::Approx(1.334).epsilon(1e-6));
    CHECK(p0.direction == 1);
    const auto p1 = ip::place_arrow(a, 1, 1, 1);
    CHECK(p1.x == doctest::Approx(1.334).epsilon(1e-6));
    CHECK(p1.y == doctest::Approx(0.833).epsilon(1e-6));
    CHECK(p1.direction == 2);
    const auto p2 = ip::place_arrow(a, 1, 1, 2);
    CHECK(p2.x == doctest::Approx(0.833).epsilon(1e-6));
    CHECK(p2.y == doctest::Approx(-0.334).epsilon(1e-6));
    CHECK(p2.direction == 3);
    const auto p3 = ip::place_arrow(a, 1, 1, 3);
    CHECK(p3.x == doctest::Approx(-0.334).epsilon(1e-6));
    CHECK(p3.y == doctest::Approx(0.167).epsilon(1e-6));
    CHECK(p3.direction == 0);
    // A 3 x 2 footprint turned once: (v, w - u).
    const auto q = ip::place_arrow(ip::EntranceArrow{8, 0, 2}, 3, 2, 1);
    CHECK(q.x == doctest::Approx(0.167).epsilon(1e-6));
    CHECK(q.y == doctest::Approx(3.0 - (2.667 + 0.167 + 0.5)).epsilon(1e-6));
}

TEST_CASE("info preview: the arrow figure's frame per direction") {
    for (unsigned d = 0; d < 4; ++d) {
        CHECK(ip::arrow_frame(d, 8) == 2 * d);  // 8 directions: shift 1
        CHECK(ip::arrow_frame(d, 16) == 4 * d); // 16: shift 0
        CHECK(ip::arrow_frame(d, 4) == d);      // 4: shift 2
    }
}

TEST_CASE("info preview: one canvas over the four turns, the current turn centred in it") {
    const auto building = solid(10, 10, 100);
    std::vector<anno::assets::Rgba> arrow_images;
    for (std::uint8_t k = 0; k < 8; ++k)
        arrow_images.push_back(solid(2, 2, static_cast<std::uint8_t>(200 + k)));
    ip::Input in;
    in.w = 1;
    in.h = 1;
    for (int r = 0; r < 4; ++r)
        in.building.push_back({&building, 5, 5});
    in.arrows = {ip::EntranceArrow{0, 2, 1}};
    for (const auto& img : arrow_images)
        in.arrow_frames.push_back({&img, 0, 0});
    // By hand: building rect (-5, 15)-(5, 25) at every turn; the arrow at (48, 30), (-20, 44),
    // (-48, 10), (20, -3). Turn widths 55, 25, 53, 27; heights 17, 31, 15, 28 -> canvas 55 x 31.
    const auto c0 = ip::compose(in, 0);
    REQUIRE(c0.has_value());
    CHECK(c0->width == 55);
    CHECK(c0->height == 31); // a per-turn fit would be 17 high
    // Turn 0: origin (-5, 15 + (17 - 31) / 2 = 8): building at (0, 7), the arrow (frame 2) at
    // (53, 22).
    CHECK(red_at(*c0, 0, 7) == 100);
    CHECK(red_at(*c0, 0, 6) == 0);
    CHECK(red_at(*c0, 53, 22) == 202);
    CHECK(red_at(*c0, 52, 22) == 0);
    // Turn 1: x -20..5 (25) in 55: origin -20 + (25 - 55) / 2 = -35; y 15..46: origin 15.
    const auto c1 = ip::compose(in, 1);
    REQUIRE(c1.has_value());
    CHECK(c1->width == 55);
    CHECK(c1->height == 31);
    CHECK(red_at(*c1, -20 + 35, 44 - 15) == 204); // the arrow, direction 2
    CHECK(red_at(*c1, -5 + 35, 0) == 100);        // the building's top-left
    // Without arrows the canvas is the building alone.
    in.arrows.clear();
    const auto plain = ip::compose(in, 0);
    REQUIRE(plain.has_value());
    CHECK(plain->width == 10);
    CHECK(plain->height == 10);
    // A turn without a building: no picture.
    in.building[3].image = nullptr;
    CHECK_FALSE(ip::compose(in, 0).has_value());
}

TEST_CASE("info preview: one scale for the whole canvas") {
    CHECK(ip::scaled_size(113, 84, 135, 135) == std::pair{113U, 84U}); // fits: scale 1
    CHECK(ip::scaled_size(135, 135, 135, 135) == std::pair{135U, 135U});
    // 270 x 100: s = 2, exact.
    CHECK(ip::scaled_size(270, 100, 135, 135) == std::pair{135U, 50U});
    // 300 x 150: s = f32(300 / 135) = 2.2222223, so 300 / s = 134.99999 -> 134, 150 / s -> 67.
    CHECK(ip::scaled_size(300, 150, 135, 135) == std::pair{134U, 67U});
    // The taller side decides: 100 x 270 -> s = 2.
    CHECK(ip::scaled_size(100, 270, 135, 135) == std::pair{50U, 135U});
}

TEST_CASE("info preview: further sprite sets - turned offset, height and draw order (OQ-712)") {
    CHECK(ip::turn_offset(1.0, 2.0, 0) == std::pair{1.0, 2.0});
    CHECK(ip::turn_offset(1.0, 2.0, 1) == std::pair{2.0, -1.0});
    CHECK(ip::turn_offset(1.0, 2.0, 2) == std::pair{-1.0, -2.0});
    CHECK(ip::turn_offset(1.0, 2.0, 3) == std::pair{-2.0, 1.0});
    // Height: 59.1633 cos(42 pi / 256) = 51.477 px per tile up; raised 1 tile at (0.5, 0.5):
    // 20.621 - 51.477 = -30.86 -> -30 (toward zero).
    CHECK(ip::project_raised(0.5, 0.5, 0.0) == ip::project(0.5, 0.5));
    CHECK(ip::project_raised(0.5, 0.5, 1.0) == std::pair{0, -30});

    // A 1 x 1 building of 10 x 10 (anchor 5, 5) and two 2 x 2 sets at the centre, height 0:
    // an animated one (red 50) listed first and a still one (red 60) second. The still one is
    // drawn first, the animated one over it.
    const auto building = solid(10, 10, 100);
    const auto anim = solid(2, 2, 50);
    const auto still = solid(2, 2, 60);
    ip::Input in;
    for (int r = 0; r < 4; ++r) {
        in.building.push_back({&building, 5, 5});
        in.sets.push_back({ip::SetPart{{&anim, 1, 1}, 0.0, 0.0, 0.0, true},
                           ip::SetPart{{&still, 1, 1}, 0.0, 0.0, 0.0, false}});
    }
    const auto c = ip::compose(in, 0);
    REQUIRE(c.has_value());
    CHECK(c->width == 10);
    CHECK(red_at(*c, 4, 4) == 50); // the animated set on top
    // A set offset 1 tile in x, raised 1 tile, at turn 0: point (1.5, 0.5, h 1):
    // x = 41.835 * (0.5 - 1.5) = -41.8 -> -41, y = 20.621 * 2 - 51.477 = -10.2 -> -10.
    in.sets.assign(4, {ip::SetPart{{&still, 0, 0}, 1.0, 1.0, 0.0, false}});
    const auto d = ip::compose(in, 0);
    REQUIRE(d.has_value());
    // Building rect (-5, 15)-(5, 25) at every turn; the set at (-41, -10), (-41, -51), (41, -51),
    // (41, -10): canvas 48 x 76. Turn 0's rect (-41, -10)-(5, 25) sits at origin
    // (-41 + (46 - 48) / 2, -10 + (35 - 76) / 2) = (-42, -30): the set at (1, 20).
    CHECK(d->width == 48);
    CHECK(d->height == 76);
    CHECK(red_at(*d, 1, 20) == 60);
    CHECK(red_at(*d, 0, 20) == 0);
}
