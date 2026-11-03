// ShipMotion: a ship sails smoothly between two published turns, never past the newest one, and
// turns in steps; the facing frame of a heading is the measured one (W4-OPENING-1, owner playtest
// 2026-11-03).
#include <cmath>
#include <set>

#include <doctest/doctest.h>

#include <anno/app/ship_layer.hpp>
#include <anno/app/ship_motion.hpp>

namespace {

namespace app = anno::app;
namespace sim = anno::sim;

app::OpeningView view_of(double x_tiles, double y_tiles, std::uint8_t dir16) {
    app::OpeningView v;
    app::ShipView s;
    s.id = sim::FigureId{1};
    s.pos.x = static_cast<std::int32_t>(std::lround(x_tiles * 256.0));
    s.pos.y = static_cast<std::int32_t>(std::lround(y_tiles * 256.0));
    s.dir16 = dir16;
    v.ships.push_back(s);
    return v;
}

double x_of(const app::OpeningView& v) {
    return static_cast<double>(v.ships.at(0).pos.x) / 256.0;
}

} // namespace

TEST_CASE("ship motion: the 16 headings of a world direction") {
    // dir16: 0 is -x, 4 is +y, 8 is +x, 12 is -y.
    CHECK(app::ShipMotion::dir16_of(-1, 0) == 0);
    CHECK(app::ShipMotion::dir16_of(0, 1) == 4);
    CHECK(app::ShipMotion::dir16_of(1, 0) == 8);
    CHECK(app::ShipMotion::dir16_of(0, -1) == 12);
    // Screen right is (+x, -y): between +x (8) and -y (12).
    CHECK(app::ShipMotion::dir16_of(1, -1) == 10);
    CHECK(app::ShipMotion::dir16_of(-1, 1) == 2);
    CHECK(app::ShipMotion::dir16_of(-1, -0.4) == 15);
}

TEST_CASE("ship motion: the sprite frame of a heading is the measured one") {
    // Measured: frame k shows its bow towards compass 45 + 22.5 k degrees, dir16 d heads 315 - 22.5
    // d.
    CHECK(app::ShipLayer::facing_frame(0, 16) == 12);
    CHECK(app::ShipLayer::facing_frame(4, 16) == 8);
    CHECK(app::ShipLayer::facing_frame(8, 16) == 4);
    CHECK(app::ShipLayer::facing_frame(12, 16) == 0);
    // A ship sailing to the screen's right heads (+x, -y) = dir16 10 and shows frame 2 (bow east).
    CHECK(app::ShipLayer::facing_frame(10, 16) == 2);
    std::set<std::size_t> frames;
    for (std::uint8_t d = 0; d < 16; ++d) {
        frames.insert(app::ShipLayer::facing_frame(d, 16));
    }
    CHECK(frames.size() == 16); // a bijection
    CHECK(app::ShipLayer::facing_frame(5, 8) == 5);
    CHECK(app::ShipLayer::facing_frame(9, 8) == 1); // a set of another size keeps the plain index
}

TEST_CASE("ship motion: positions move along the segment, never past the newest turn") {
    app::ShipMotion m;
    m.observe(view_of(10, 20, 8), 0.0);
    CHECK(x_of(m.display(view_of(10, 20, 8), 0.0)) == doctest::Approx(10.0));
    // The next turn, 0.1 s later, is one tile on.
    const app::OpeningView second = view_of(11, 20, 8);
    m.observe(second, 0.1);
    CHECK(x_of(m.display(second, 0.1)) == doctest::Approx(10.0)); // starts where it was shown
    CHECK(x_of(m.display(second, 0.15)) == doctest::Approx(10.5).epsilon(0.01));
    CHECK(x_of(m.display(second, 0.2)) == doctest::Approx(11.0).epsilon(0.01));
    CHECK(x_of(m.display(second, 0.5)) == doctest::Approx(11.0).epsilon(0.001)); // no overshoot
    CHECK_FALSE(m.animating(0.5));
    // A turn arriving mid-segment continues from where the ship is shown: no jump back.
    const app::OpeningView third = view_of(12, 20, 8);
    app::ShipMotion n;
    n.observe(view_of(10, 20, 8), 0.0);
    n.observe(second, 0.1);
    const double shown = x_of(n.display(second, 0.15));
    n.observe(third, 0.15);
    CHECK(x_of(n.display(third, 0.15)) == doctest::Approx(shown).epsilon(0.001));
    CHECK(n.animating(0.16));
}

TEST_CASE("ship motion: a ship that appears or jumps is shown where it is") {
    app::ShipMotion m;
    m.observe(view_of(5, 5, 4), 0.0);
    const app::OpeningView far = view_of(200, 5, 4);
    m.observe(far, 0.1);
    CHECK(x_of(m.display(far, 0.1)) == doctest::Approx(200.0));
}

TEST_CASE("ship motion: the facing turns in steps along the short way and follows the movement") {
    app::ShipMotion m;
    m.observe(view_of(0, 0, 8), 0.0);
    // Moving towards -y: heading 12.
    app::OpeningView v = view_of(0, -0.3, 8);
    m.observe(v, 0.1);
    CHECK(m.display(v, 0.1).ships[0].dir16 == 8); // the first frame keeps the facing it had
    // 4 steps to go, one per 0.06 s: never more than one step at a time.
    std::uint8_t last = 8;
    int steps = 0;
    for (double t = 0.1; t < 0.6; t += 0.02) {
        const std::uint8_t d = m.display(v, t).ships[0].dir16;
        const int diff = (d - last + 16) & 15;
        CHECK((diff == 0 || diff == 1));
        steps += diff;
        last = d;
    }
    CHECK(last == 12);
    CHECK(steps == 4);
    // Short way round: from 15 to 1 goes through 0.
    app::ShipMotion w;
    w.observe(view_of(0, 0, 15), 0.0);
    app::OpeningView u = view_of(-0.3, 0.12, 15); // heading about 1 (west-south-west)
    w.observe(u, 0.1);
    const std::uint8_t target = app::ShipMotion::dir16_of(-0.3, 0.12);
    CHECK(((target - 15 + 16) & 15) <= 3);
    std::set<std::uint8_t> seen;
    for (double t = 0.1; t < 0.5; t += 0.01) {
        seen.insert(w.display(u, t).ships[0].dir16);
    }
    CHECK(seen.count(0) == 1);
    CHECK(seen.count(8) == 0); // never the long way
}

TEST_CASE("ship motion: a ship that does not move shows the sim's facing") {
    app::ShipMotion m;
    m.observe(view_of(3, 3, 2), 0.0);
    app::OpeningView same = view_of(3, 3, 6); // turned in place by the sim
    m.observe(same, 0.1);
    std::uint8_t d = 0;
    for (double t = 0.1; t < 0.6; t += 0.02) {
        d = m.display(same, t).ships[0].dir16;
    }
    CHECK(d == 6);
}
