// Tests of the river raster and the flow solved on it (docs/render/rivers.md
// 2.3-2.4, 3.2, 10.1; stage S4). Synthetic rivers only: no game files.
//
// Every case builds a RiverMask by hand (a channel of wet cells with a bed and a
// surface level, dry banks, sea cells as outlets) and checks a law of the model:
// continuity (the flux through a cross-section is the lateral inflow of the
// area upstream of it), the conveyance exponent, the discharge models, the
// outlets, falls, ponds, the fallback and determinism. The helper `Chan`
// paints rectangles; sections are columns of the raster (rivers run along +x).
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/render/iso.hpp>
#include <anno/render/rect.hpp>
#include <anno/render/river.hpp>

namespace river = anno::render::river;
using river::kBridge;
using river::kFall;
using river::kMouth;
using river::kSea;
using river::kWet;
using river::RiverConfig;
using river::RiverFlow;
using river::RiverMask;

namespace {

constexpr std::int32_t kC = 8; // cells per tile

/// The model without the visual post-processing, so that laws hold exactly:
/// no soft floor, no cap, no smoothing, no log.
RiverConfig exact() {
    RiverConfig c;
    c.speed_floor = 0.0F;
    c.speed_cap = 1000.0F;
    c.smoothing_passes = 0;
    c.log_fallback = false;
    return c;
}

/// A raster with painting helpers.
struct Chan {
    RiverMask m;
    Chan(std::int32_t nx, std::int32_t ny)
        : m(static_cast<std::uint32_t>(kC), 0, 0, static_cast<std::uint32_t>(nx),
            static_cast<std::uint32_t>(ny)) {}
    /// Wet rectangle [x0, x1) x [y0, y1) of depth `depth` (HEIGHT units) under
    /// the level `level`.
    void water(std::int32_t x0, std::int32_t y0, std::int32_t x1, std::int32_t y1, float depth,
               float level = 10.0F, std::uint8_t extra = 0) {
        for (std::int32_t y = y0; y < y1; ++y) {
            for (std::int32_t x = x0; x < x1; ++x) {
                m.set_wet(x, y, level - depth, level, extra);
            }
        }
    }
    void sea(std::int32_t x0, std::int32_t y0, std::int32_t x1, std::int32_t y1) {
        for (std::int32_t y = y0; y < y1; ++y) {
            for (std::int32_t x = x0; x < x1; ++x) {
                m.set_sea(x, y);
            }
        }
    }
};

/// A straight channel along +x: columns [8, 8 + len), rows [16, 16 + w), a sea
/// strip behind its end, a closed head at the start.
Chan straight(std::int32_t len, std::int32_t w, float depth, float level = 10.0F) {
    Chan c(len + 40, w + 32);
    c.water(8, 16, 8 + len, 16 + w, depth, level);
    c.sea(8 + len, 16, 8 + len + 4, 16 + w);
    return c;
}

/// Approx with a purely relative tolerance (doctest's default scale of 1 makes
/// the tolerance absolute for values near 1).
doctest::Approx rel(double v, double eps) {
    return doctest::Approx(v).epsilon(eps).scale(0.0);
}

const RiverFlow::Cell& cell(const RiverFlow& f, std::int32_t cx, std::int32_t cy) {
    const std::size_t k = f.cell_at(cx, cy);
    REQUIRE(k != RiverFlow::npos);
    return f.cells()[k];
}

/// Discharge (cells^3 .. in tile units: tiles^3 / s) through the column cx:
/// the sum of u_x * depth over its wet cells times the cell width.
double flux(const RiverFlow& f, std::int32_t cx) {
    double q = 0.0;
    for (const auto& c : f.cells()) {
        if (c.cx == cx) {
            q += static_cast<double>(c.ux) * static_cast<double>(c.depth) / kC;
        }
    }
    return q;
}

/// Wet cells with column <= cx (the area that feeds the section, in cells).
double area_upstream(const RiverFlow& f, std::int32_t cx) {
    double a = 0.0;
    for (const auto& c : f.cells()) {
        a += c.cx <= cx ? 1.0 : 0.0;
    }
    return a;
}

double mean_speed_where(const RiverFlow& f, auto pred) {
    double s = 0.0;
    std::size_t n = 0;
    for (const auto& c : f.cells()) {
        if (pred(c)) {
            s += std::hypot(static_cast<double>(c.ux), static_cast<double>(c.uy));
            ++n;
        }
    }
    return n > 0 ? s / static_cast<double>(n) : 0.0;
}

} // namespace

TEST_CASE("river flow: a straight channel flows along its axis at the outlet speed") {
    const Chan c = straight(320, 64, 6.0F);
    RiverConfig cfg = exact();
    const RiverFlow f = RiverFlow::build(c.m, cfg);
    const auto& st = f.stats();
    CHECK(st.wet_cells == 320U * 64U);
    CHECK(st.components == 1);
    CHECK(st.ponds == 0);
    CHECK(st.sea_less_components == 0);
    CHECK_FALSE(st.fell_back);
    CHECK(st.residual < 1e-4);
    CHECK(st.non_finite == 0);
    // Along the axis: the core (3 tiles clear of the banks, 8 tiles from the head)
    // within 2 degrees.
    std::size_t core = 0, off = 0;
    for (const auto& q : f.cells()) {
        const bool in_core = q.cy >= 16 + 24 && q.cy < 16 + 64 - 24 && q.cx > 8 + 64;
        if (!in_core) {
            continue;
        }
        ++core;
        const double ang =
            std::atan2(std::fabs(static_cast<double>(q.uy)), static_cast<double>(q.ux));
        off += !(q.ux > 0.0F && ang < 2.0 * 3.14159265 / 180.0);
    }
    CHECK(core > 1000);
    CHECK(off == 0);
    // Uniform in the middle of the channel: the core of the section at its
    // half length varies by less than 3 % (the bank profile reaches 2 tiles).
    double lo = 1e9, hi = 0.0, sum = 0.0;
    std::size_t n = 0;
    for (std::int32_t y = 16 + 16; y < 16 + 64 - 16; ++y) {
        const double u = cell(f, 8 + 160, y).ux;
        lo = std::min(lo, u), hi = std::max(hi, u), sum += u, ++n;
    }
    CHECK((hi - lo) / (sum / static_cast<double>(n)) < 0.03);
    // The mean speed over the outlet cells is the outlet speed of the
    // Normalised discharge model (rivers.md E7).
    const double u_out =
        mean_speed_where(f, [](const RiverFlow::Cell& q) { return q.cx == 8 + 319; });
    CHECK(u_out == rel(static_cast<double>(cfg.out_speed), 0.03));
    // Outlet cells are flagged: exactly the last column.
    for (const auto& q : f.cells()) {
        CHECK(((q.flags & RiverFlow::kOutlet) != 0) == (q.cx == 8 + 319));
    }
    CHECK(st.outlet_faces == 64);
}

TEST_CASE("river flow: continuity, the flux through a section is the inflow upstream of it") {
    // Uniform lateral inflow r: Q(x) = r * area upstream of x (rivers.md E3),
    // through every section of a straight channel.
    const Chan c = straight(320, 64, 6.0F);
    const RiverFlow f = RiverFlow::build(c.m, exact());
    std::vector<double> ratio;
    for (const std::int32_t x : {8 + 80, 8 + 140, 8 + 200, 8 + 260, 8 + 316}) {
        ratio.push_back(flux(f, x) / area_upstream(f, x));
    }
    const auto [lo, hi] = std::minmax_element(ratio.begin(), ratio.end());
    CHECK(*hi / *lo < 1.01);
    CHECK(*lo > 0.0);
}

TEST_CASE("river flow: taper and depth step keep the discharge continuous") {
    // Wide (12 tiles) -> narrow (6 tiles) over 10 tiles, and a step from 8 to
    // 4 HEIGHT units depth at the end of the taper: Q(x) = r * A_up(x) holds
    // in every section; the speed rises where the channel narrows and shoals.
    Chan c(380, 128);
    for (std::int32_t x = 8; x < 340; ++x) {
        const double t = std::clamp((x - 120.0) / 80.0, 0.0, 1.0);
        const auto hw = static_cast<std::int32_t>(std::lround(48.0 - 24.0 * t));
        const float depth = x < 200 ? 8.0F : 4.0F;
        c.water(x, 64 - hw, x + 1, 64 + hw, depth);
    }
    c.sea(340, 40, 344, 88);
    const RiverFlow f = RiverFlow::build(c.m, exact());
    REQUIRE(f.stats().fallback_components == 0);
    std::vector<double> ratio;
    for (const std::int32_t x : {60, 100, 150, 190, 215, 260, 300, 335}) {
        ratio.push_back(flux(f, x) / area_upstream(f, x));
    }
    const auto [lo, hi] = std::minmax_element(ratio.begin(), ratio.end());
    CHECK(*hi / *lo < 1.03);
    // Core speed rises through the narrowing and the shoal.
    const double wide = mean_speed_where(
        f, [](const RiverFlow::Cell& q) { return q.cx == 100 && std::abs(q.cy - 64) < 16; });
    const double narrow = mean_speed_where(
        f, [](const RiverFlow::Cell& q) { return q.cx == 300 && std::abs(q.cy - 64) < 16; });
    CHECK(narrow > 1.5 * wide);
}

TEST_CASE("river flow: the conveyance goes with depth to the power 5/3") {
    // One channel, bed in two depths side by side (4 and 8 units, 6 tiles
    // each, 24 cells away from the step at the sample point): the potential is
    // common to both halves in the far field, so the unit discharge q = u h
    // splits like the conveyance K ~ h^e and the speeds like h^(e-1)
    // (Manning: e = 5/3, ratio 2^(2/3) = 1.587; Chezy e = 3/2 would give 1.414).
    Chan c(400, 144);
    c.water(8, 16, 360, 64, 4.0F);
    c.water(8, 64, 360, 112, 8.0F);
    c.sea(360, 16, 364, 112);
    {
        const RiverFlow f = RiverFlow::build(c.m, exact());
        const double shallow = cell(f, 200, 40).ux, deep = cell(f, 200, 88).ux;
        CHECK(deep / shallow == rel(std::pow(2.0, 2.0 / 3.0), 0.02));
    }
    {
        RiverConfig chezy = exact();
        chezy.conveyance_exp = 1.5F;
        const RiverFlow f = RiverFlow::build(c.m, chezy);
        const double shallow = cell(f, 200, 40).ux, deep = cell(f, 200, 88).ux;
        CHECK(deep / shallow == rel(std::pow(2.0, 0.5), 0.02));
    }
}

TEST_CASE("river flow: roughness cancels out of the Normalised discharge") {
    // k_M = 1 / (n ell^(1/3)) scales the potential, not the flux (rivers.md
    // 2.3: only the direction and the ratios of q are physics).
    const Chan c = straight(160, 32, 6.0F);
    RiverConfig a = exact(), b = exact();
    b.manning_n = 0.08F;
    b.metres_per_tile = 1.3F;
    const RiverFlow fa = RiverFlow::build(c.m, a), fb = RiverFlow::build(c.m, b);
    REQUIRE(fa.cells().size() == fb.cells().size());
    for (std::size_t i = 0; i < fa.cells().size(); ++i) {
        CHECK(fa.cells()[i].ux == rel(fb.cells()[i].ux, 1e-4));
    }
}

TEST_CASE("river flow: the banks slow the margin") {
    // E6: the roughness near a bank slows the margin, the core carries the flux.
    const Chan c = straight(320, 64, 6.0F);
    const RiverFlow f = RiverFlow::build(c.m, exact());
    const double core = cell(f, 8 + 160, 16 + 32).ux;
    const double margin = cell(f, 8 + 160, 16).ux;
    // Bank cell centre at 1/16 tile: nu = 1 + 4 exp(-0.0625 / 0.3) = 4.2 against 1.
    CHECK(margin < 0.45 * core);
    CHECK(margin > 0.0);
}

TEST_CASE("river flow: an opening to the sea is not a bank") {
    // The sea is no wall: the flow keeps its profile up to the outlet. The core
    // speed of the last column follows the growth of the discharge (Q ~ x) from
    // 3 tiles upstream; with the sea taken for a bank the roughness would slow
    // the last tiles to a quarter.
    const Chan c = straight(320, 64, 6.0F);
    const RiverFlow f = RiverFlow::build(c.m, exact());
    const double last = cell(f, 8 + 319, 16 + 32).ux, before = cell(f, 8 + 319 - 24, 16 + 32).ux;
    CHECK(last / before == rel(319.5 / 295.5, 0.03));
    // And the head (a closed end) is a wall: its margin is slower than its core.
    CHECK(cell(f, 8 + 6, 16 + 32).ux > 1.5 * cell(f, 8 + 1, 16 + 32).ux);
}

TEST_CASE("river flow: the 3 x 3 smoothing softens the field and keeps its mean") {
    // A sharp depth step (no bed smoothing) makes a jump of speed between two
    // cells; one smoothing pass spreads it over three.
    Chan c(240, 64);
    c.water(8, 16, 120, 40, 4.0F);
    c.water(120, 16, 232, 40, 9.0F);
    c.sea(232, 16, 236, 40);
    RiverConfig sharp = exact();
    sharp.bed_sigma_tiles = 0.01F;
    RiverConfig soft = sharp;
    soft.smoothing_passes = 1;
    const RiverFlow a = RiverFlow::build(c.m, sharp), b = RiverFlow::build(c.m, soft);
    auto jump = [](const RiverFlow& f) {
        double worst = 0.0;
        for (std::int32_t x = 100; x < 140; ++x) {
            const double u0 = cell(f, x, 28).ux, u1 = cell(f, x + 1, 28).ux;
            worst = std::max(worst, std::fabs(u0 - u1) / std::max(u0, u1));
        }
        return worst;
    };
    CHECK(jump(a) > 0.25);
    CHECK(jump(b) < 0.9 * jump(a));
    CHECK(b.stats().speed_mean == rel(static_cast<double>(a.stats().speed_mean), 0.03));
}

TEST_CASE("river flow: a Y confluence conserves the discharge") {
    // Two arms (depths 8 and 4 units) that only meet at a junction feed one trunk;
    // each carries the inflow of its own area, the trunk the sum plus its own.
    Chan c(380, 128);
    c.water(8, 8, 160, 40, 8.0F);    // arm A
    c.water(8, 88, 160, 120, 4.0F);  // arm B
    c.water(160, 8, 200, 120, 6.0F); // junction
    c.water(200, 48, 340, 80, 6.0F); // trunk
    c.sea(340, 48, 344, 80);
    const RiverFlow f = RiverFlow::build(c.m, exact());
    REQUIRE(f.stats().components == 1);
    const double r = flux(f, 330) / area_upstream(f, 330);
    auto arm_area = [&](bool upper, std::int32_t x) {
        double a = 0.0;
        for (const auto& q : f.cells()) {
            a += (q.cx <= x && q.cx < 160 && (q.cy < 64) == upper) ? 1.0 : 0.0;
        }
        return a;
    };
    auto arm_flux = [&](bool upper, std::int32_t x) {
        double s = 0.0;
        for (const auto& q : f.cells()) {
            if (q.cx == x && (q.cy < 64) == upper) {
                s += static_cast<double>(q.ux) * static_cast<double>(q.depth) / kC;
            }
        }
        return s;
    };
    const double qa = arm_flux(true, 150), qb = arm_flux(false, 150);
    CHECK(qa == rel(r * arm_area(true, 150), 0.01));
    CHECK(qb == rel(r * arm_area(false, 150), 0.01));
    // Trunk = both arms + the inflow of everything between the sections.
    double between = 0.0;
    for (const auto& q : f.cells()) {
        between += (q.cx > 150 && q.cx <= 300) ? 1.0 : 0.0;
    }
    CHECK(flux(f, 300) == rel(qa + qb + r * between, 0.01));
}

TEST_CASE("river flow: both arms of a braid flow downstream") {
    // A long head reach (the through flux dominates the arms' own inflow), a
    // short straight arm A and a long detour B (down, along, back up), both
    // joining at the same two junctions. An Eikonal (distance to the outlet)
    // field sends the lower part of the detour back to the first junction;
    // the potential flow does not (rivers.md 2.3).
    Chan c(760, 128);
    c.water(8, 56, 420, 72, 6.0F); // head reach
    c.water(420, 56, 432, 72, 6.0F);
    c.water(432, 56, 520, 72, 6.0F); // arm A
    c.water(432, 8, 444, 56, 6.0F);  // B1: the detour leaves the head reach downwards (-y)
    c.water(432, 8, 520, 24, 6.0F);  // B2
    c.water(508, 8, 520, 56, 6.0F);  // B3: back up (+y)
    c.water(520, 56, 700, 72, 6.0F); // outlet reach
    c.sea(700, 56, 704, 72);
    const RiverFlow f = RiverFlow::build(c.m, exact());
    REQUIRE(f.stats().components == 1);
    auto share = [&](auto region, auto ok) {
        std::size_t good = 0, n = 0;
        for (const auto& q : f.cells()) {
            if (region(q)) {
                ++n;
                good += ok(q) ? 1U : 0U;
            }
        }
        REQUIRE(n > 100);
        return static_cast<double>(good) / static_cast<double>(n);
    };
    auto in_a = [](const RiverFlow::Cell& q) {
        return q.cx >= 440 && q.cx < 512 && q.cy >= 58 && q.cy < 70;
    };
    auto in_b1 = [](const RiverFlow::Cell& q) {
        return q.cx >= 434 && q.cx < 442 && q.cy >= 12 && q.cy < 52;
    };
    auto in_b2 = [](const RiverFlow::Cell& q) {
        return q.cx >= 450 && q.cx < 502 && q.cy >= 10 && q.cy < 22;
    };
    auto in_b3 = [](const RiverFlow::Cell& q) {
        return q.cx >= 510 && q.cx < 518 && q.cy >= 12 && q.cy < 52;
    };
    CHECK(share(in_a, [](const RiverFlow::Cell& q) { return q.ux > 0.0F; }) > 0.99);
    CHECK(share(in_b1, [](const RiverFlow::Cell& q) { return q.uy < 0.0F; }) > 0.99);
    CHECK(share(in_b2, [](const RiverFlow::Cell& q) { return q.ux > 0.0F; }) > 0.99);
    CHECK(share(in_b3, [](const RiverFlow::Cell& q) { return q.uy > 0.0F; }) > 0.99);
    // The Eikonal fallback (forced) is wrong there: it sends part of B1 and
    // B3 backwards. This is why the fallback is visible, not silent.
    RiverConfig forced = exact();
    forced.cg_max_iterations = 1;
    forced.cg_tolerance = 1e-14;
    forced.residual_limit = 1e-12;
    const RiverFlow e = RiverFlow::build(c.m, forced);
    REQUIRE(e.stats().fell_back);
    std::size_t wrong = 0;
    for (const auto& q : e.cells()) {
        wrong += in_b1(q) && q.uy >= 0.0F;
    }
    CHECK(wrong > 20);
}

TEST_CASE("river flow: a 90 degree bend follows the banks") {
    // East along a 4-tile channel, then south: a streamline from the middle of
    // the first leg stays inside the water through the corner (no corner
    // cutting) and reaches the outlet; the cells at the banks of the legs flow
    // along them. Documented limitation (rivers.md 2.5): potential flow is
    // fastest at the inner corner, the opposite of nature.
    Chan c(240, 240);
    c.water(8, 8, 200, 40, 6.0F);    // leg 1 (along +x)
    c.water(168, 8, 200, 208, 6.0F); // leg 2 (along +y)
    c.sea(168, 208, 200, 212);
    RiverConfig cfg = exact();
    const RiverFlow f = RiverFlow::build(c.m, cfg);
    REQUIRE(f.stats().components == 1);
    // Streamline.
    double x = 40.0 / kC, y = 24.0 / kC;
    double min_valid = 1.0, path = 0.0;
    int steps = 0;
    while (y < 205.0 / kC && steps < 20000) {
        const auto s1 = f.sample(static_cast<float>(x), static_cast<float>(y));
        const double sp = std::hypot(static_cast<double>(s1.u[0]), static_cast<double>(s1.u[1]));
        REQUIRE(sp > 0.0);
        const double h = 0.04;
        const double mx = x + 0.5 * h * s1.u[0] / sp, my = y + 0.5 * h * s1.u[1] / sp;
        const auto s2 = f.sample(static_cast<float>(mx), static_cast<float>(my));
        const double sp2 = std::hypot(static_cast<double>(s2.u[0]), static_cast<double>(s2.u[1]));
        x += h * s2.u[0] / sp2;
        y += h * s2.u[1] / sp2;
        path += h;
        min_valid = std::min(min_valid, static_cast<double>(s2.valid));
        ++steps;
    }
    CHECK(steps < 20000);
    CHECK(min_valid > 0.9);
    // Ends in leg 2 (x within the channel), travelled about the centreline length.
    CHECK(x > 168.0 / kC);
    CHECK(x < 200.0 / kC);
    CHECK(path == rel((184.0 - 40.0 + 200.0 - 24.0) / kC, 0.08));
    // Banks: in the straight part of leg 1 the cells next to the walls flow
    // along them (the wall-normal component is a small part of the speed).
    double worst = 0.0;
    for (const auto& q : f.cells()) {
        if (q.cx > 8 + 40 && q.cx < 168 - 24 && (q.cy == 8 || q.cy == 39)) {
            const double sp = std::hypot(static_cast<double>(q.ux), static_cast<double>(q.uy));
            worst = std::max(worst, std::fabs(static_cast<double>(q.uy)) / sp);
        }
    }
    CHECK(worst < 0.15);
    // Inner corner (about (168, 40)) is faster than the outer corner (199, 8).
    auto speed_near = [&](std::int32_t cx, std::int32_t cy) {
        return mean_speed_where(f, [&](const RiverFlow::Cell& q) {
            return std::abs(q.cx - cx) <= 3 && std::abs(q.cy - cy) <= 3;
        });
    };
    CHECK(speed_near(171, 37) > speed_near(196, 11));
}

TEST_CASE("river flow: falls run at the fall speed, downhill") {
    // Level 64 -> 16 over 3 tiles (16 units per tile: full rapids,
    // rivers.md E10) in a 3-tile wide channel with the outlet at the low end.
    Chan c(360, 64);
    for (std::int32_t x = 8; x < 328; ++x) {
        const double t = std::clamp((x - 120.0) / 24.0, 0.0, 1.0);
        const float level = static_cast<float>(64.0 - 48.0 * t);
        c.water(x, 16, x + 1, 40, 6.0F, level);
    }
    c.sea(328, 16, 332, 40);
    RiverConfig cfg = exact();
    cfg.speed_cap = 3.0F;
    const RiverFlow f = RiverFlow::build(c.m, cfg);
    std::size_t steep = 0, downhill = 0, flat_fast = 0;
    for (const auto& q : f.cells()) {
        const double sp = std::hypot(static_cast<double>(q.ux), static_cast<double>(q.uy));
        if (q.slope > anno::render::kRapidsFrom) {
            ++steep;
            downhill += q.ux > 0.0F;
        }
        if (q.slope >= anno::render::kRapidsFull) {
            CHECK(sp >= static_cast<double>(cfg.fall_speed) - 1e-3);
            CHECK(sp <= static_cast<double>(cfg.speed_cap) + 1e-3);
        }
        if (q.slope < 1e-3F && std::abs(q.cx - 132) > 40) {
            flat_fast += sp >= static_cast<double>(cfg.fall_speed);
        }
    }
    CHECK(steep > 200);
    CHECK(static_cast<double>(downhill) >= 0.99 * static_cast<double>(steep));
    CHECK(flat_fast == 0);
    // The slope itself: the level falls 16 units per tile in the middle.
    CHECK(cell(f, 132, 28).slope == rel(16.0, 0.1));
}

TEST_CASE("river flow: the quantised painted bed does not make blocky speeds") {
    // A bed of 3 x 3-tile blocks in steps of 4 HEIGHT units (the data's
    // quantisation, rivers.md F-j), pits of depth 0 included: no jump of speed
    // between neighbouring cells above 10 % (in the core), never shallower than
    // h_min.
    Chan c(360, 80);
    const std::array<float, 7> bed = {0.0F, 4.0F, 0.0F, 4.0F, 8.0F, 4.0F, 0.0F};
    for (std::int32_t x = 8; x < 328; ++x) {
        const std::size_t block = static_cast<std::size_t>((x - 8) / 24) % bed.size();
        c.water(x, 24, x + 1, 56, 10.0F - bed[block], 10.0F);
    }
    c.sea(328, 24, 332, 56);
    RiverConfig cfg = exact();
    const RiverFlow f = RiverFlow::build(c.m, cfg);
    double worst = 0.0;
    for (std::int32_t y = 24 + 8; y < 56 - 8; ++y) {
        for (std::int32_t x = 8 + 24; x < 328 - 24; ++x) {
            const double a = cell(f, x, y).ux, b = cell(f, x + 1, y).ux;
            worst = std::max(worst, std::fabs(a - b) / std::max(a, b));
        }
    }
    CHECK(worst < 0.10);
    for (const auto& q : f.cells()) {
        CHECK(q.depth >= river::kUnitTiles * cfg.h_min - 1e-6F);
        CHECK(q.depth <= river::kUnitTiles * cfg.h_max + 1e-6F);
    }
    // The smoothing is what does it: without it (a very small sigma) the steps show.
    RiverConfig sharp = exact();
    sharp.bed_sigma_tiles = 0.01F;
    const RiverFlow g = RiverFlow::build(c.m, sharp);
    double worst_sharp = 0.0;
    for (std::int32_t y = 24 + 8; y < 56 - 8; ++y) {
        for (std::int32_t x = 8 + 24; x < 328 - 24; ++x) {
            const double a = cell(g, x, y).ux, b = cell(g, x + 1, y).ux;
            worst_sharp = std::max(worst_sharp, std::fabs(a - b) / std::max(a, b));
        }
    }
    CHECK(worst_sharp > 0.3);
}

TEST_CASE("river flow: ponds are not solved, they swirl") {
    Chan c(60, 40);
    c.water(10, 10, 13, 13, 6.0F); // 9 cells: a pond
    c.water(30, 10, 34, 13, 6.0F); // 12 cells: a river (no sea: pseudo outlet)
    const RiverConfig cfg = exact();
    const RiverFlow f = RiverFlow::build(c.m, cfg);
    CHECK(f.stats().ponds == 1);
    CHECK(f.stats().components == 1);
    CHECK(f.stats().sea_less_components == 1);
    for (const auto& q : f.cells()) {
        CHECK(std::isfinite(q.ux));
        CHECK(std::isfinite(q.uy));
        if (q.cx < 20) {
            CHECK(q.river == RiverFlow::kPond);
            CHECK(std::hypot(static_cast<double>(q.ux), static_cast<double>(q.uy)) ==
                  rel(static_cast<double>(cfg.pond_swirl), 1e-3));
        } else {
            CHECK(q.river == 0);
        }
    }
    // The swirl is not one constant vector.
    bool varies = false;
    for (const auto& q : f.cells()) {
        varies = varies || std::fabs(q.ux - f.cells()[0].ux) > 1e-4F;
    }
    CHECK(varies);
}

TEST_CASE("river flow: rivers drain at the sea on either side") {
    // Sea at both ends: a divide in the middle, every cell flows to its nearer end.
    Chan c(360, 64);
    c.water(8, 16, 328, 40, 6.0F);
    c.sea(4, 16, 8, 40);
    c.sea(328, 16, 332, 40);
    const RiverFlow f = RiverFlow::build(c.m, exact());
    CHECK(f.stats().outlet_faces == 48);
    std::size_t left_ok = 0, left = 0, right_ok = 0, right = 0;
    for (const auto& q : f.cells()) {
        if (q.cx < 8 + 150) {
            ++left, left_ok += q.ux < 0.0F;
        } else if (q.cx > 8 + 170) {
            ++right, right_ok += q.ux > 0.0F;
        }
    }
    CHECK(left_ok == left);
    CHECK(right_ok == right);
    // And the speed grows towards both ends, not towards the middle.
    CHECK(std::fabs(cell(f, 20, 28).ux) > std::fabs(cell(f, 100, 28).ux));
    CHECK(std::fabs(cell(f, 320, 28).ux) > std::fabs(cell(f, 250, 28).ux));
}

TEST_CASE("river flow: a river without sea drains at its lowest level") {
    // Decision D14: no sea contact -> a pseudo outlet at the lowest level, then
    // the lowest bed; mouths (kMouth) are preferred.
    auto build = [](auto paint) {
        Chan c(360, 64);
        paint(c);
        RiverFlow f = RiverFlow::build(c.m, exact());
        // The pseudo outlet is an outlet like the sea's: the discharge that
        // enters the river leaves it there (continuity), and the solve is sound.
        CHECK(f.stats().flux_imbalance < 1e-3);
        CHECK(f.stats().residual < 1e-3);
        CHECK_FALSE(f.stats().fell_back);
        return f;
    };
    auto share_right = [](const RiverFlow& f) {
        std::size_t good = 0, n = 0;
        for (const auto& q : f.cells()) {
            if (q.cx > 8 + 60 && q.cx < 8 + 280) {
                ++n, good += q.ux > 0.0F;
            }
        }
        return static_cast<double>(good) / static_cast<double>(n);
    };
    {
        // The level falls to the right.
        const RiverFlow f = build([](Chan& c) {
            for (std::int32_t x = 8; x < 328; ++x) {
                c.water(x, 16, x + 1, 40, 6.0F, 20.0F - 12.0F * static_cast<float>(x - 8) / 320.0F);
            }
        });
        CHECK(f.stats().sea_less_components == 1);
        CHECK(f.stats().outlet_faces == 0);
        CHECK(share_right(f) > 0.99);
        bool pseudo_right = false, pseudo_left = false;
        for (const auto& q : f.cells()) {
            if ((q.flags & RiverFlow::kPseudo) != 0) {
                (q.cx > 160 ? pseudo_right : pseudo_left) = true;
            }
        }
        CHECK(pseudo_right);
        CHECK_FALSE(pseudo_left);
    }
    {
        // The level falls to the left.
        const RiverFlow f = build([](Chan& c) {
            for (std::int32_t x = 8; x < 328; ++x) {
                c.water(x, 16, x + 1, 40, 6.0F, 8.0F + 12.0F * static_cast<float>(x - 8) / 320.0F);
            }
        });
        CHECK(share_right(f) < 0.01);
    }
    {
        // Flat level, the bed falls to the right.
        const RiverFlow f = build([](Chan& c) {
            for (std::int32_t x = 8; x < 328; ++x) {
                const float depth = 4.0F + 6.0F * static_cast<float>(x - 8) / 320.0F;
                c.water(x, 16, x + 1, 40, depth, 20.0F);
            }
        });
        CHECK(share_right(f) > 0.99);
    }
    {
        // The level has priority over the bed: the surface is lowest at the
        // left end but the bed is lowest (a deep pool) at the right end, under
        // a higher surface. Water follows the surface, so the river drains to
        // the left; picking the lowest bed would send it to the right.
        const RiverFlow f = build([](Chan& c) {
            for (std::int32_t x = 8; x < 328; ++x) {
                const float t = static_cast<float>(x - 8) / 320.0F;
                c.water(x, 16, x + 1, 40, 4.0F + 8.0F * t, 10.0F + 2.0F * t);
            }
        });
        CHECK(f.stats().sea_less_components == 1);
        CHECK(share_right(f) < 0.01);
        bool pseudo_right = false;
        for (const auto& q : f.cells()) {
            pseudo_right = pseudo_right || ((q.flags & RiverFlow::kPseudo) != 0 && q.cx > 160);
        }
        CHECK_FALSE(pseudo_right);
    }
    {
        // A mouth piece at the left end wins over a lower level on the right.
        const RiverFlow f = build([](Chan& c) {
            c.water(8, 16, 328, 40, 6.0F, 20.0F);
            c.water(8, 16, 24, 40, 6.0F, 20.0F, kMouth);
            c.water(300, 16, 328, 40, 6.0F, 18.0F);
        });
        CHECK(share_right(f) < 0.01);
    }
}

TEST_CASE("river flow: discharge models") {
    const Chan c = straight(320, 32, 6.0F);
    auto u_at = [](const RiverFlow& f, std::int32_t cx) {
        return mean_speed_where(f, [cx](const RiverFlow::Cell& q) { return q.cx == cx; });
    };
    RiverConfig n = exact();
    n.discharge = RiverConfig::Discharge::Normalised;
    // Normalised: a longer river has the same speed at its mouth.
    const Chan shorter = straight(160, 32, 6.0F);
    CHECK(u_at(RiverFlow::build(c.m, n), 8 + 319) ==
          rel(u_at(RiverFlow::build(shorter.m, n), 8 + 159), 0.02));
    // Absolute: u_out = inflow * length / depth (Q = r W L, A = W h), and the
    // longer river is faster by the length ratio.
    RiverConfig a = exact();
    a.discharge = RiverConfig::Discharge::Absolute;
    const RiverFlow fa = RiverFlow::build(c.m, a);
    const RiverFlow fs = RiverFlow::build(shorter.m, a);
    const double depth_tiles = 6.0 * static_cast<double>(river::kUnitTiles);
    const double expected = static_cast<double>(a.inflow_rate) * (320.0 / kC) / depth_tiles;
    CHECK(u_at(fa, 8 + 319) == rel(expected, 0.03));
    CHECK(u_at(fa, 8 + 319) / u_at(fs, 8 + 159) == rel(2.0, 0.03));
    RiverConfig a2 = a;
    a2.inflow_rate = 2.0F * a.inflow_rate;
    CHECK(u_at(RiverFlow::build(c.m, a2), 8 + 319) / u_at(fa, 8 + 319) == rel(2.0, 0.01));
    // Sourced: the inflow enters at the head only, so beyond it the speed is
    // constant along the river (Q / A) at about source_speed, where the
    // lateral inflow of the other models grows linearly.
    RiverConfig s = exact();
    s.discharge = RiverConfig::Discharge::Sourced;
    const RiverFlow fq = RiverFlow::build(c.m, s);
    // Section speed Q / A (the cell mean is lower: the banks slow the margin).
    const double section = 32.0 / kC * depth_tiles;
    const double u30 = flux(fq, 8 + 96) / section, u90 = flux(fq, 8 + 288) / section;
    CHECK(u90 / u30 == rel(1.0, 0.03));
    CHECK(u30 == rel(static_cast<double>(s.source_speed), 0.10));
    // The lateral inflow of Absolute grows with the area upstream: x3 over the same sections.
    CHECK(flux(fa, 8 + 288) / flux(fa, 8 + 96) == rel(3.0, 0.03));
    CHECK(fq.stats().flux_imbalance < 1e-3);
    CHECK(fa.stats().flux_imbalance < 1e-3);
}

TEST_CASE("river flow: the soft floor, the cap and the smoothing") {
    const Chan c = straight(320, 32, 6.0F);
    RiverConfig base = exact();
    const RiverFlow raw = RiverFlow::build(c.m, base);
    // Floor: a speed below it is lifted, (u^4 + f^4)^(1/4); never below f.
    RiverConfig fl = exact();
    fl.speed_floor = 0.3F;
    const RiverFlow f = RiverFlow::build(c.m, fl);
    for (std::size_t i = 0; i < f.cells().size(); ++i) {
        const double u = std::hypot(static_cast<double>(raw.cells()[i].ux),
                                    static_cast<double>(raw.cells()[i].uy));
        const double v =
            std::hypot(static_cast<double>(f.cells()[i].ux), static_cast<double>(f.cells()[i].uy));
        const double want = std::pow(u * u * u * u + std::pow(0.3, 4.0), 0.25);
        CHECK(v == rel(want, 0.003));
        CHECK(v >= 0.3 - 1e-5);
    }
    // Cap: nothing above it.
    RiverConfig cap = exact();
    cap.speed_cap = 0.4F;
    const RiverFlow g = RiverFlow::build(c.m, cap);
    double hi = 0.0;
    for (const auto& q : g.cells()) {
        hi = std::max(hi, std::hypot(static_cast<double>(q.ux), static_cast<double>(q.uy)));
    }
    CHECK(hi <= 0.4 + 1e-5);
    CHECK(hi > 0.399); // the outlet end would be faster: it sits on the cap
    // Smoothing keeps the field finite, inside the range, and does not move the mean.
    RiverConfig sm = exact();
    sm.smoothing_passes = 2;
    const RiverFlow h = RiverFlow::build(c.m, sm);
    CHECK(h.stats().speed_mean == rel(raw.stats().speed_mean, 0.02));
}

TEST_CASE("river flow: a failed solve falls back to the Eikonal field, visibly") {
    const Chan c = straight(320, 32, 6.0F);
    RiverConfig cfg = exact();
    cfg.cg_max_iterations = 1;
    cfg.cg_tolerance = 1e-14;
    cfg.residual_limit = 1e-9;
    const RiverFlow f = RiverFlow::build(c.m, cfg);
    CHECK(f.stats().fell_back);
    CHECK(f.stats().fallback_components == 1);
    CHECK(f.stats().non_finite == 0);
    std::size_t right = 0;
    for (const auto& q : f.cells()) {
        CHECK((q.flags & RiverFlow::kFallback) != 0);
        CHECK(std::isfinite(q.ux));
        right += q.ux > 0.0F;
        // The Eikonal speed is the outlet speed.
        CHECK(std::hypot(static_cast<double>(q.ux), static_cast<double>(q.uy)) ==
              rel(static_cast<double>(cfg.out_speed), 0.02));
    }
    CHECK(right >= f.cells().size() * 99 / 100);
    // A normal run does not fall back and reports its residual.
    const RiverFlow g = RiverFlow::build(c.m, exact());
    CHECK_FALSE(g.stats().fell_back);
    CHECK(g.stats().fallback_components == 0);
    CHECK(g.stats().residual > 0.0);
    CHECK(g.stats().residual <= 1e-3);
}

TEST_CASE("river flow: builds share no state, one thread or many") {
    // A build is a pure function of its mask and config (no static or shared
    // state, a fixed summation order): the same input gives the same field
    // when built again or by many threads at once. The result is bit-identical
    // per toolchain only (libm pow / exp / cbrt / hypot differ between MSVC
    // and glibc), so it carries no golden digest: S5 digests quantise the
    // lattice first (rivers.md 3.3, R9: 1/4096 tiles per second).
    Chan c(500, 200);
    c.water(8, 16, 300, 40, 6.0F);
    c.water(100, 40, 130, 180, 5.0F);
    c.water(100, 150, 480, 180, 7.0F);
    c.sea(480, 150, 484, 180);
    c.water(150, 90, 153, 93, 4.0F); // a pond
    const RiverConfig cfg;
    const RiverFlow ref = RiverFlow::build(c.m, cfg);
    REQUIRE(ref.cells().size() > 1000);
    auto same = [&](const RiverFlow& g) {
        if (g.cells().size() != ref.cells().size()) {
            return false;
        }
        // Field by field, not memcmp: the struct has padding bytes.
        for (std::size_t i = 0; i < ref.cells().size(); ++i) {
            const RiverFlow::Cell &a = g.cells()[i], &b = ref.cells()[i];
            if (a.cx != b.cx || a.cy != b.cy || a.ux != b.ux || a.uy != b.uy ||
                a.depth != b.depth || a.river != b.river || a.slope != b.slope ||
                a.flags != b.flags || a.turb != b.turb || a.wake != b.wake ||
                a.froude != b.froude) {
                return false;
            }
        }
        return true;
    };
    CHECK(same(RiverFlow::build(c.m, cfg)));
    std::vector<RiverFlow> built(8);
    std::vector<std::thread> pool;
    for (std::size_t i = 0; i < built.size(); ++i) {
        pool.emplace_back([&, i] { built[i] = RiverFlow::build(c.m, cfg); });
    }
    for (auto& t : pool) {
        t.join();
    }
    for (const auto& g : built) {
        CHECK(same(g));
    }
}

TEST_CASE("river flow: sampling is a valid-weighted bilinear field") {
    const Chan c = straight(160, 32, 6.0F);
    const RiverFlow f = RiverFlow::build(c.m, RiverConfig{});
    // At a cell centre: the cell's own velocity.
    const auto& q = cell(f, 60, 30);
    const auto s = f.sample((60.5F) / kC, (30.5F) / kC);
    CHECK(s.valid == doctest::Approx(1.0F));
    CHECK(s.u[0] == rel(q.ux, 1e-5));
    CHECK(s.u[1] == rel(q.uy, 1e-5));
    // Halfway between two cells: their mean.
    const auto& r = cell(f, 61, 30);
    const auto m = f.sample(61.0F / kC, 30.5F / kC);
    CHECK(m.u[0] == rel(0.5F * (q.ux + r.ux), 1e-5));
    // Outside the water: nothing valid. At the rim: a full vector from the
    // half that is wet, not dragged to zero by the dry side.
    CHECK(f.sample(60.0F / kC, 5.0F / kC).valid == 0.0F);
    const auto& top = cell(f, 60, 16);
    const auto rim = f.sample(60.5F / kC, 16.0F / kC); // on the edge of the cell row
    CHECK(rim.valid == rel(0.5F, 0.01));
    CHECK(rim.u[0] == rel(top.ux, 1e-4));
}

TEST_CASE("river mask: the shipped criteria on the tile grid") {
    // The synthetic island of the bed test: a channel one corner wide along y
    // at x = 6, banks 16 units up, the river surface 9 above the lowest corner
    // (sea::kRiverLevel). The raster must classify every cell as
    // sea::water_field classifies a pixel there.
    namespace sea = anno::render::sea;
    constexpr std::size_t n = 14;
    sea::IslandWater iw;
    iw.w = iw.h = n;
    iw.heights.assign(n * n, 40);
    for (std::size_t y = 0; y < n; ++y) {
        iw.heights[y * n + 6] = 24;
        iw.heights[y * n + 7] = 24; // a channel two corners wide: water over the tile between
    }
    iw.sea_chunk.assign(4, false);
    iw.open.assign(n * n, 0.0F);
    iw.shore.assign(n * n, 0.0F);
    iw.river.assign(n * n, std::numeric_limits<float>::quiet_NaN());
    const std::vector<sea::RiverPiece> pieces = {{5, 2, 3, 9, 24}};
    for (std::size_t y = 2; y <= 11; ++y) {
        for (std::size_t x = 5; x <= 8; ++x) {
            iw.river[y * n + x] = 24.0F + sea::kRiverLevel;
        }
    }
    RiverConfig cfg;
    const RiverMask m = RiverMask::from_island(iw, pieces, {}, cfg);
    REQUIRE(m.nx() > 0);
    std::size_t wet = 0, checked = 0;
    for (std::int32_t cy = m.y0(); cy < m.y0() + static_cast<std::int32_t>(m.ny()); ++cy) {
        for (std::int32_t cx = m.x0(); cx < m.x0() + static_cast<std::int32_t>(m.nx()); ++cx) {
            const float x = (static_cast<float>(cx) + 0.5F) / static_cast<float>(kC);
            const float y = (static_cast<float>(cy) + 0.5F) / static_cast<float>(kC);
            const sea::SceneProbe probe{{0}, {{x, y}}};
            const auto field = sea::water_field(anno::render::Rect{0, 0, 1, 1}, probe,
                                                [&iw](std::size_t) { return &iw; });
            ++checked;
            const bool river = (field.flags[0] & 2U) != 0;
            CHECK(m.wet(cx, cy) == river);
            wet += river;
            if (m.wet(cx, cy)) {
                // Level and bed are the field's: depth = level - bed > -edge.
                const std::size_t i = m.index(cx, cy);
                CHECK(m.level[i] - m.bed[i] > -anno::render::kWaves.shore_edge);
            }
        }
    }
    CHECK(checked > 1000);
    CHECK(wet > 100); // the channel exists
    // Mutation guard: the channel is wet along its axis and dry on the banks.
    CHECK(m.wet(6 * kC + 4, 6 * kC));
    CHECK_FALSE(m.wet(3 * kC, 6 * kC));
}

TEST_CASE("river mask: falls and mouths join the rivers that meet at them") {
    // Two beds 1.5 tiles apart (a cliff between them hides the terrain, so the
    // fall's sprite covers no cell in between): a fall piece over the gap
    // links them with a channel flagged kBridge; a bed piece or a gap longer
    // than bridge_tiles does not.
    namespace sea = anno::render::sea;
    constexpr std::size_t n = 24;
    sea::IslandWater iw;
    iw.w = iw.h = n;
    iw.heights.assign(n * n, 60);
    for (std::size_t y = 6; y <= 10; ++y) {
        for (std::size_t x = 2; x <= 6; ++x) {
            iw.heights[y * n + x] = 24;
        }
        for (std::size_t x = 8; x <= 12; ++x) {
            iw.heights[y * n + x] = 24;
        }
    }
    iw.sea_chunk.assign(9, false);
    iw.open.assign(n * n, 0.0F);
    iw.shore.assign(n * n, 0.0F);
    iw.river.assign(n * n, std::numeric_limits<float>::quiet_NaN());
    const std::vector<sea::RiverPiece> pieces = {{2, 6, 4, 4, 24}, {8, 6, 4, 4, 24}};
    for (std::size_t y = 6; y <= 10; ++y) {
        for (std::size_t x = 2; x <= 6; ++x) {
            iw.river[y * n + x] = 24.0F + sea::kRiverLevel;
        }
        for (std::size_t x = 8; x <= 12; ++x) {
            iw.river[y * n + x] = 24.0F + sea::kRiverLevel;
        }
    }
    const RiverConfig cfg;
    auto cover = [&](river::SpriteCover::Kind kind) {
        river::SpriteCover s;
        s.kind = kind;
        s.height = 24;
        s.tx0 = 4, s.ty0 = 5, s.tx1 = 10, s.ty1 = 11;
        s.cells.assign(static_cast<std::size_t>(6 * kC) * static_cast<std::size_t>(6 * kC), 0);
        return s;
    };
    auto count_flag = [](const RiverMask& m, std::uint8_t flag) {
        std::size_t k = 0;
        for (const std::uint8_t f : m.flags) {
            k += (f & flag) != 0;
        }
        return k;
    };
    {
        const std::vector<river::SpriteCover> covers = {cover(river::SpriteCover::Kind::Fall)};
        const RiverMask m = RiverMask::from_island(iw, pieces, covers, cfg);
        CHECK(m.bridges == 1);
        CHECK(count_flag(m, kBridge) > 20);
        const RiverFlow f = RiverFlow::build(m, cfg);
        CHECK(f.stats().components == 1); // one river through the fall
    }
    {
        const std::vector<river::SpriteCover> covers = {cover(river::SpriteCover::Kind::Mouth)};
        CHECK(RiverMask::from_island(iw, pieces, covers, cfg).bridges == 1);
    }
    {
        const std::vector<river::SpriteCover> covers = {cover(river::SpriteCover::Kind::Bed)};
        const RiverMask m = RiverMask::from_island(iw, pieces, covers, cfg);
        CHECK(m.bridges == 0);
        CHECK(count_flag(m, kBridge) == 0);
        CHECK(RiverFlow::build(m, cfg).stats().components == 2);
    }
    {
        RiverConfig tight = cfg;
        tight.bridge_tiles = 0.5F; // the gap is 1.5 tiles
        const std::vector<river::SpriteCover> covers = {cover(river::SpriteCover::Kind::Fall)};
        CHECK(RiverMask::from_island(iw, pieces, covers, tight).bridges == 0);
        RiverConfig off = cfg;
        off.bridge_tiles = 0.0F;
        CHECK(RiverMask::from_island(iw, pieces, covers, off).bridges == 0);
    }
}

TEST_CASE("river mask: sprite cover never turns the original's sea into river") {
    // Where the original draws sea (terrain under the waterline in a sea chunk) the
    // sprites of a river mouth do not make river water (Scene::apply_fall_masks keeps
    // those pixels sea); on land the same cover is the painted bed.
    namespace sea = anno::render::sea;
    constexpr std::size_t n = 16;
    sea::IslandWater iw;
    iw.w = iw.h = n;
    iw.heights.assign(n * n, 60);
    for (std::size_t y = 0; y < n; ++y) {
        for (std::size_t x = 9; x < n; ++x) {
            iw.heights[y * n + x] = 0; // sea on the right
        }
    }
    iw.sea_chunk.assign(4, true);
    iw.open.assign(n * n, 1.0F);
    iw.shore.assign(n * n, 0.0F);
    iw.river.assign(n * n, std::numeric_limits<float>::quiet_NaN());
    const std::vector<sea::RiverPiece> pieces = {{3, 6, 3, 3, 60}};
    river::SpriteCover s;
    s.kind = river::SpriteCover::Kind::Mouth;
    s.height = 60;
    s.tx0 = 2, s.ty0 = 5, s.tx1 = 14, s.ty1 = 10;
    s.cells.assign(static_cast<std::size_t>(12 * kC) * static_cast<std::size_t>(5 * kC), 1);
    RiverConfig cfg;
    cfg.bridge_tiles = 0.0F;
    const RiverMask m = RiverMask::from_island(iw, {pieces}, {&s, 1}, cfg);
    std::size_t land_wet = 0, sea_wet = 0, sea_marked = 0;
    for (std::int32_t cy = 5 * kC; cy < 10 * kC; ++cy) {
        for (std::int32_t cx = 2 * kC; cx < 14 * kC; ++cx) {
            const bool in_sea = cx >= 9 * kC + 4; // well inside the sea tiles
            const bool on_land = cx < 8 * kC - 4;
            if (in_sea) {
                sea_wet += m.wet(cx, cy);
                sea_marked += (m.flags_at(cx, cy) & kSea) != 0;
            } else if (on_land) {
                land_wet += m.wet(cx, cy);
            }
        }
    }
    CHECK(sea_wet == 0);
    CHECK(sea_marked > 100);
    CHECK(land_wet > 500);
}

TEST_CASE("river flow: the face conductance is the harmonic mean of its two cells") {
    // Two shallow necks (1.5 units, one cell wide) join the same deep pools, one 3
    // cells long, one 6. A face between a deep and a shallow cell is a series of
    // two half cells, resistance (1/K_deep + 1/K_shallow) / 2, about half a
    // neck cell: the necks resist like 3 and 6 cells, the flux splits 2 : 1. The
    // arithmetic mean (K_deep + K_shallow) / 2 would make the faces free and the
    // split 2.4 : 1.
    Chan c(110, 80);
    c.water(8, 20, 40, 60, 12.0F, 12.0F);  // upstream pool
    c.water(46, 20, 90, 60, 12.0F, 12.0F); // downstream pool
    c.water(43, 28, 46, 33, 12.0F, 12.0F); // a bay of it in front of the short neck
    c.water(40, 30, 43, 31, 1.5F, 12.0F);  // short neck, 3 cells
    c.water(40, 50, 46, 51, 1.5F, 12.0F);  // long neck, 6 cells
    c.sea(90, 20, 94, 60);
    RiverConfig cfg = exact();
    cfg.bed_sigma_tiles = 0.0F;
    const RiverFlow f = RiverFlow::build(c.m, cfg);
    REQUIRE(f.stats().components == 1);
    auto neck_flux = [&](std::int32_t cx, std::int32_t cy) {
        const auto& q = cell(f, cx, cy);
        return static_cast<double>(q.ux) * static_cast<double>(q.depth) / kC;
    };
    const double short_neck = neck_flux(41, 30), long_neck = neck_flux(43, 50);
    REQUIRE(short_neck > 0.0);
    REQUIRE(long_neck > 0.0);
    CHECK(short_neck / long_neck == rel(2.0, 0.06));
}

// ---------------------------------------------------------------------------
// Review of stage S4 (2026-10-19): the rules the first review found
// unprotected (discharge reference, pseudo-outlet priority, enhanced sea,
// bridges, multigrid efficiency, the sprite projection).
// ---------------------------------------------------------------------------

namespace {

constexpr std::int32_t kBlobRow0 = 16, kBlobRow1 = 112;  // the sea-facing blob, 12 tiles wide
constexpr std::int32_t kRiverRow0 = 56, kRiverRow1 = 72; // the river proper, 2 tiles wide
constexpr std::int32_t kBlobLen = 48;                    // 6 tiles deep

/// A river 2 tiles wide and `len` cells long that ends in a broad blob facing
/// the sea (the shape of the original's river-mouth sprites, rivers.md 14.2).
/// mid_neck: a bottleneck of that many cells width half way along, 8 cells
/// long; mouth_neck: a neck of that width over the last 4 cells.
Chan blob_river(std::int32_t len, std::int32_t mid_neck, std::int32_t mouth_neck) {
    Chan c(len + kBlobLen + 60, 128);
    const std::int32_t xe = 8 + len;
    auto reach = [&](std::int32_t xa, std::int32_t xb, std::int32_t w) {
        const std::int32_t y0 = (kRiverRow0 + kRiverRow1) / 2 - w / 2;
        c.water(xa, y0, xb, y0 + w, 6.0F);
    };
    const std::int32_t w = kRiverRow1 - kRiverRow0;
    if (mid_neck > 0) {
        reach(8, 8 + len / 2, w);
        reach(8 + len / 2, 8 + len / 2 + 8, mid_neck);
        reach(8 + len / 2 + 8, xe, w);
    } else if (mouth_neck > 0) {
        reach(8, xe - 4, w);
        reach(xe - 4, xe, mouth_neck);
    } else {
        reach(8, xe, w);
    }
    c.water(xe, kBlobRow0, xe + kBlobLen, kBlobRow1, 6.0F);
    c.sea(xe + kBlobLen, kBlobRow0, xe + kBlobLen + 4, kBlobRow1);
    return c;
}

} // namespace

TEST_CASE("river flow: a broad mouth does not set the speed scale (Normalised, OQ-065)") {
    // The original's mouths are blobs up to six times wider than the river in
    // front of them. The reference speed is measured at the fastest section
    // that passes half of the discharge, not at the sea faces: the river proper
    // runs at about out_speed (growing towards its mouth with the area it
    // drains), the blob is slower, and next to nothing sits on the cap.
    const std::int32_t len = 320, xe = 8 + len;
    struct Variant {
        const char* name;
        std::int32_t mid_neck, mouth_neck;
    };
    // A 5-cell bottleneck half way: it carries 26 % of the discharge, below
    // out_flux_share, so it is not the reference although it is the fastest
    // section. (A pinch that passes half of the discharge, wide enough or not,
    // does set the scale: the price of "fastest cross-section", OQ-065;
    // Reference::MedianCell does not have it.)
    for (const Variant& v : {Variant{"plain", 0, 0}, Variant{"bottleneck", 5, 0}}) {
        INFO("variant: " << std::string(v.name));
        const Chan c = blob_river(len, v.mid_neck, v.mouth_neck);
        const RiverConfig cfg = exact();
        const RiverFlow f = RiverFlow::build(c.m, cfg);
        REQUIRE(f.stats().components == 1);
        CHECK(f.stats().residual < 1e-4);
        const double u_river = mean_speed_where(f, [&](const RiverFlow::Cell& q) {
            return q.cx >= xe - 56 && q.cx < xe - 40 && q.cy >= kRiverRow0 && q.cy < kRiverRow1;
        });
        const double u_blob = mean_speed_where(f, [&](const RiverFlow::Cell& q) {
            return q.cx >= xe + 8 && q.cx < xe + kBlobLen - 8;
        });
        // The speed grows linearly along the river with the area drained, so
        // 5 to 7 tiles above the mouth it is 0.81 to 0.88 of the outlet speed.
        CHECK(u_river / static_cast<double>(cfg.out_speed) > 0.70);
        CHECK(u_river / static_cast<double>(cfg.out_speed) < 0.95);
        // The blob has the discharge of the river over six times the width.
        CHECK(u_blob < 0.5 * u_river);
        // The default look: floor, cap and smoothing on; the cap holds nothing.
        RiverConfig def;
        def.log_fallback = false;
        const RiverFlow g = RiverFlow::build(c.m, def);
        CHECK(g.stats().cap_fraction < 0.10F);
        CHECK(g.stats().speed_max <= def.speed_cap + 1e-4F);
    }
    // The parameters at work: no section as wide as out_min_cells (nor one
    // passing the share) leaves only the sea faces as the reference (the first
    // measurement, rivers.md E7): the river proper is then several times too
    // fast, as the share 0 / min 1 / faces regressions would make it.
    const Chan c = blob_river(len, 0, 0);
    RiverConfig faces = exact();
    faces.out_min_cells = 1000;
    const RiverFlow f = RiverFlow::build(c.m, faces);
    const double u_faces = mean_speed_where(f, [&](const RiverFlow::Cell& q) {
        return q.cx >= xe - 56 && q.cx < xe - 40 && q.cy >= kRiverRow0 && q.cy < kRiverRow1;
    });
    CHECK(u_faces / static_cast<double>(faces.out_speed) > 2.0);
    RiverConfig share_zero = exact();
    share_zero.out_flux_share = 0.0F;
    const Chan neck = blob_river(len, 5, 0);
    const RiverFlow g = RiverFlow::build(neck.m, share_zero);
    const double u_share = mean_speed_where(g, [&](const RiverFlow::Cell& q) {
        return q.cx >= xe - 56 && q.cx < xe - 40 && q.cy >= kRiverRow0 && q.cy < kRiverRow1;
    });
    CHECK(u_share / static_cast<double>(share_zero.out_speed) < 0.6);
}

TEST_CASE("river flow: the median reference sets the speed of the median cell") {
    // OQ-065 option: Reference::MedianCell, a flatter, livelier field.
    const Chan c = blob_river(320, 0, 0);
    RiverConfig cfg = exact();
    cfg.reference = RiverConfig::Reference::MedianCell;
    cfg.median_speed = 0.4F;
    const RiverFlow f = RiverFlow::build(c.m, cfg);
    std::vector<float> speeds;
    for (const auto& q : f.cells()) {
        speeds.push_back(std::hypot(q.ux, q.uy));
    }
    std::nth_element(speeds.begin(),
                     speeds.begin() + static_cast<std::ptrdiff_t>(speeds.size() / 2), speeds.end());
    CHECK(speeds[speeds.size() / 2] == rel(0.4, 0.05));
    // The default reference leaves the median far below.
    const RiverFlow d = RiverFlow::build(c.m, exact());
    CHECK(d.stats().speed_p50 < 0.3F);
    // The median of the default look cannot fall below the floor.
    RiverConfig look;
    look.log_fallback = false;
    const RiverFlow e = RiverFlow::build(c.m, look);
    CHECK(e.stats().floor_fraction >= 0.0F);
    CHECK(e.stats().floor_fraction <= 1.0F);
    look.reference = RiverConfig::Reference::MedianCell;
    const RiverFlow m = RiverFlow::build(c.m, look);
    CHECK(m.stats().floor_fraction < e.stats().floor_fraction);
}

TEST_CASE("river flow: a divide is a slow line and both sides run away from it") {
    // Review S4: where the solved flux is nearly zero (the divide of a river
    // with two outlets) the direction is only noise; with the floor on, the
    // speed there must not jump up to an arbitrary direction.
    Chan c(360, 64);
    c.water(8, 16, 328, 40, 6.0F);
    c.sea(4, 16, 8, 40);
    c.sea(328, 16, 332, 40);
    RiverConfig cfg;
    cfg.log_fallback = false;
    const RiverFlow f = RiverFlow::build(c.m, cfg);
    auto speed_in = [&](std::int32_t x0, std::int32_t x1) {
        return mean_speed_where(f,
                                [&](const RiverFlow::Cell& q) { return q.cx >= x0 && q.cx < x1; });
    };
    const double divide = speed_in(8 + 156, 8 + 164);
    const double arm_left = speed_in(8 + 60, 8 + 80), arm_right = speed_in(8 + 240, 8 + 260);
    CHECK(divide < 0.6 * arm_left);
    CHECK(divide < 0.6 * arm_right);
    // Away from the divide on both sides; every direction finite.
    std::size_t left = 0, left_ok = 0, right = 0, right_ok = 0;
    for (const auto& q : f.cells()) {
        CHECK(std::isfinite(q.ux));
        CHECK(std::isfinite(q.uy));
        if (q.cx >= 8 + 120 && q.cx < 8 + 152) {
            ++left, left_ok += q.ux < 0.0F;
        } else if (q.cx >= 8 + 168 && q.cx < 8 + 200) {
            ++right, right_ok += q.ux > 0.0F;
        }
    }
    CHECK(left_ok == left);
    CHECK(right_ok == right);
}

TEST_CASE("river flow: the transition passes soften the speed at a fall") {
    // Review S4: the speed rises from the river's to the fall's within a few
    // cells; speed-weighted extra passes over the cells near kFall / kMouth /
    // kBridge soften the jump and keep the field's mean.
    Chan c(360, 64);
    for (std::int32_t x = 8; x < 328; ++x) {
        const double t = std::clamp((x - 160.0) / 8.0, 0.0, 1.0);
        const bool in_fall = x >= 160 && x < 168;
        c.water(x, 16, x + 1, 40, 6.0F, static_cast<float>(40.0 - 16.0 * t), in_fall ? kFall : 0);
    }
    c.sea(328, 16, 332, 40);
    RiverConfig plain;
    plain.log_fallback = false;
    plain.transition_passes = 0;
    RiverConfig soft = plain;
    soft.transition_passes = 4;
    const RiverFlow a = RiverFlow::build(c.m, plain), b = RiverFlow::build(c.m, soft);
    auto worst_jump = [&](const RiverFlow& f) {
        double worst = 0.0;
        for (std::int32_t x = 140; x < 190; ++x) {
            for (std::int32_t y = 22; y < 34; ++y) {
                const auto &p = cell(f, x, y), &q = cell(f, x + 1, y);
                worst = std::max(worst, std::hypot(static_cast<double>(q.ux - p.ux),
                                                   static_cast<double>(q.uy - p.uy)));
            }
        }
        return worst;
    };
    CHECK(worst_jump(b) < 0.8 * worst_jump(a));
    CHECK(b.stats().speed_mean == rel(a.stats().speed_mean, 0.05));
    // And the fall itself is still fast.
    CHECK(cell(b, 164, 28).ux > 1.0F);
    // No transition cells, no change.
    Chan flat = straight(200, 32, 6.0F);
    const RiverFlow fa = RiverFlow::build(flat.m, plain), fb = RiverFlow::build(flat.m, soft);
    for (std::size_t i = 0; i < fa.cells().size(); ++i) {
        CHECK(fa.cells()[i].ux == fb.cells()[i].ux);
    }
}

TEST_CASE("river flow: the multigrid solver stays fast and its coarse fallback works") {
    // Efficiency is a property of the preconditioner (rivers.md 14.1: 17-98
    // iterations for 4-56 K cells, 5-7 levels): bound it, or a silent loss of
    // multigrid quality only shows up as seconds in the budget gate.
    {
        const Chan c = straight(320, 64, 6.0F);
        const RiverFlow f = RiverFlow::build(c.m, exact());
        INFO("straight 320 x 64: " << f.stats().iterations << " iterations, " << f.stats().levels
                                   << " levels");
        CHECK(f.stats().iterations <= 28);
        CHECK(f.stats().levels >= 4);
        CHECK(f.stats().residual < 1e-4);
    }
    {
        // The braid of the test above (760 x 128 cells).
        Chan c(760, 128);
        c.water(8, 56, 420, 72, 6.0F);
        c.water(420, 56, 432, 72, 6.0F);
        c.water(432, 56, 520, 72, 6.0F);
        c.water(432, 8, 444, 56, 6.0F);
        c.water(432, 8, 520, 24, 6.0F);
        c.water(508, 8, 520, 56, 6.0F);
        c.water(520, 56, 700, 72, 6.0F);
        c.sea(700, 56, 704, 72);
        const RiverFlow f = RiverFlow::build(c.m, exact());
        INFO("braid 760 x 128: " << f.stats().iterations << " iterations, " << f.stats().levels
                                 << " levels");
        CHECK(f.stats().iterations <= 30);
        CHECK(f.stats().levels >= 4);
        CHECK(f.stats().residual < 1e-4);
    }
    {
        // No dense factor: the coarsest level is smoothed by damped Jacobi
        // sweeps (a path no corpus island reaches). The solve must still
        // converge to the same answer.
        const Chan c = straight(320, 64, 6.0F);
        const RiverFlow ref = RiverFlow::build(c.m, exact());
        RiverConfig cfg = exact();
        cfg.dense_limit = 0;
        const RiverFlow f = RiverFlow::build(c.m, cfg);
        INFO("Jacobi coarse level: " << f.stats().iterations << " iterations, " << f.stats().levels
                                     << " levels");
        CHECK_FALSE(f.stats().fell_back);
        CHECK(f.stats().residual < 1e-3);
        CHECK(f.stats().iterations <= 34);
        double worst = 0.0;
        for (std::size_t i = 0; i < ref.cells().size(); ++i) {
            worst = std::max(worst, std::fabs(static_cast<double>(f.cells()[i].ux) -
                                              static_cast<double>(ref.cells()[i].ux)));
        }
        CHECK(worst < 0.01 * static_cast<double>(RiverConfig{}.out_speed));
    }
    {
        // A single level with Jacobi sweeps only still solves a small river.
        const Chan c = straight(80, 16, 6.0F);
        RiverConfig cfg = exact();
        cfg.max_levels = 1;
        cfg.dense_limit = 0;
        const RiverFlow f = RiverFlow::build(c.m, cfg);
        CHECK_FALSE(f.stats().fell_back);
        CHECK(f.stats().levels == 1);
        CHECK(f.stats().residual < 1e-3);
        // And with the dense factor on the single level it is a direct solve.
        cfg.dense_limit = 3000;
        const RiverFlow d = RiverFlow::build(c.m, cfg);
        CHECK(d.stats().iterations <= 2);
        CHECK(d.stats().residual < 1e-8);
    }
}

namespace {

namespace sea = anno::render::sea;

/// A 14 x 14 corner island: a channel two corners wide along y at x = 6, 7
/// (terrain 24, river surface 24 + kRiverLevel over it, pieces from y = 2 to
/// `end`), and a lagoon of terrain 0 (below the waterline 16) from row `end`
/// on, 8 corners wide. sea_chunk true: the original draws the lagoon as sea;
/// false: only the enhanced waterline does.
sea::IslandWater lagoon_island(std::size_t end, bool original_sea) {
    constexpr std::size_t n = 14;
    sea::IslandWater iw;
    iw.w = iw.h = n;
    iw.heights.assign(n * n, 40);
    for (std::size_t y = 0; y < n; ++y) {
        for (std::size_t x = 6; x <= 7; ++x) {
            iw.heights[y * n + x] = 24;
        }
        if (y >= end) {
            for (std::size_t x = 3; x <= 10; ++x) {
                iw.heights[y * n + x] = 0;
            }
        }
    }
    iw.sea_chunk.assign(4, original_sea);
    iw.open.assign(n * n, 0.0F);
    iw.shore.assign(n * n, 0.0F);
    iw.river.assign(n * n, std::numeric_limits<float>::quiet_NaN());
    for (std::size_t y = 2; y <= end; ++y) {
        for (std::size_t x = 5; x <= 8; ++x) {
            iw.river[y * n + x] = 24.0F + sea::kRiverLevel;
        }
    }
    return iw;
}

std::size_t sea_cells_below(const RiverMask& m, std::int32_t cy_min) {
    std::size_t k = 0;
    for (std::int32_t cy = cy_min; cy < m.y0() + static_cast<std::int32_t>(m.ny()); ++cy) {
        for (std::int32_t cx = m.x0(); cx < m.x0() + static_cast<std::int32_t>(m.nx()); ++cx) {
            k += (m.flags_at(cx, cy) & kSea) != 0;
        }
    }
    return k;
}

} // namespace

TEST_CASE("river mask: the enhanced waterline and the original sea are outlets, seen within the "
          "margin") {
    // Review S4. A river that ends in a lagoon: the sea is classified by the
    // original's water (terrain under the waterline in a sea chunk) or by the
    // enhanced waterline (smooth terrain under it, sd > 0), whichever; the rivers
    // then have outlet faces. The classification reaches mask_margin_tiles
    // beyond the pieces.
    constexpr std::size_t end = 9;
    for (const bool original : {false, true}) {
        INFO("original sea: " << original);
        const sea::IslandWater iw = lagoon_island(end, original);
        const std::vector<sea::RiverPiece> pieces = {
            {5, 2, 3, static_cast<std::uint8_t>(end - 2), 24}};
        const RiverConfig cfg;
        const RiverMask m = RiverMask::from_island(iw, pieces, {}, cfg);
        REQUIRE(m.nx() > 0);
        // Sea cells beyond the end of the footprint (half a tile past it and
        // more): only the margin makes the raster reach there.
        CHECK(sea_cells_below(m, static_cast<std::int32_t>(end) * kC + kC / 2) > 40);
        RiverConfig cfg_flow = cfg;
        cfg_flow.log_fallback = false;
        const RiverFlow f = RiverFlow::build(m, cfg_flow);
        REQUIRE(f.stats().components >= 1);
        CHECK(f.stats().outlet_faces > 0);
        CHECK(f.stats().sea_less_components == 0);
        // Without the margin the raster ends at the footprint and sees less.
        RiverConfig none = cfg;
        none.mask_margin_tiles = 0.0F;
        const RiverMask n = RiverMask::from_island(iw, pieces, {}, none);
        CHECK(sea_cells_below(n, static_cast<std::int32_t>(end) * kC + kC / 2) == 0);
        CHECK(sea_cells_below(n, 0) < sea_cells_below(m, 0));
    }
}

namespace {

/// The island of the bridge test: two beds (terrain `bed_height`) 1.5 tiles
/// apart with `gap_height` between, a cliff of 60 around.
sea::IslandWater gap_island(std::uint8_t bed_height, std::uint8_t gap_height, bool gap_is_sea) {
    constexpr std::size_t n = 24;
    sea::IslandWater iw;
    iw.w = iw.h = n;
    iw.heights.assign(n * n, 60);
    for (std::size_t y = 6; y <= 10; ++y) {
        for (std::size_t x = 2; x <= 6; ++x) {
            iw.heights[y * n + x] = bed_height;
        }
        for (std::size_t x = 8; x <= 12; ++x) {
            iw.heights[y * n + x] = bed_height;
        }
        iw.heights[y * n + 7] = gap_height;
    }
    iw.sea_chunk.assign(9, gap_is_sea);
    iw.open.assign(n * n, 0.0F);
    iw.shore.assign(n * n, 0.0F);
    iw.river.assign(n * n, std::numeric_limits<float>::quiet_NaN());
    for (std::size_t y = 6; y <= 10; ++y) {
        for (std::size_t x = 2; x <= 6; ++x) {
            iw.river[y * n + x] = 33.0F;
        }
        for (std::size_t x = 8; x <= 12; ++x) {
            iw.river[y * n + x] = 33.0F;
        }
    }
    return iw;
}

river::SpriteCover empty_fall() {
    river::SpriteCover s;
    s.kind = river::SpriteCover::Kind::Fall;
    s.height = 24;
    s.tx0 = 4, s.ty0 = 5, s.tx1 = 10, s.ty1 = 11;
    s.cells.assign(static_cast<std::size_t>(6 * kC) * static_cast<std::size_t>(6 * kC), 0);
    return s;
}

} // namespace

TEST_CASE("river mask: a bridge never crosses sea and is deep enough to carry flow") {
    const std::vector<sea::RiverPiece> pieces = {{2, 6, 4, 4, 24}, {8, 6, 4, 4, 24}};
    const std::vector<river::SpriteCover> covers = {empty_fall()};
    {
        // Sea between the beds (the original's water in the gap tile): the link
        // is counted but nothing of the sea is carved over.
        const sea::IslandWater iw = gap_island(24, 0, true);
        const RiverConfig cfg;
        const RiverMask m = RiverMask::from_island(iw, pieces, covers, cfg);
        RiverConfig off = cfg;
        off.bridge_tiles = 0.0F;
        const RiverMask plain = RiverMask::from_island(iw, pieces, covers, off);
        auto count = [](const RiverMask& mask, std::uint8_t flag) {
            std::size_t k = 0;
            for (const std::uint8_t f : mask.flags) {
                k += (f & flag) != 0;
            }
            return k;
        };
        std::size_t both = 0;
        for (const std::uint8_t f : m.flags) {
            both += (f & kSea) != 0 && (f & (kWet | kBridge)) != 0;
        }
        REQUIRE(count(plain, kSea) > 50);
        CHECK(m.bridges == 1);
        CHECK(both == 0);
        // Not one sea cell was carved over.
        CHECK(count(m, kSea) == count(plain, kSea));
    }
    {
        // Shallow beds (terrain 31 under the surface 33: depth 2 units): the
        // carved channel is at least bridge_min_depth deep so that it carries
        // flow, and exactly that where the beds are shallower.
        const sea::IslandWater iw = gap_island(31, 31, false);
        for (const float min_depth : {3.0F, 5.0F}) {
            RiverConfig cfg;
            cfg.bridge_min_depth = min_depth;
            const RiverMask m = RiverMask::from_island(iw, pieces, covers, cfg);
            REQUIRE(m.bridges == 1);
            std::size_t carved = 0;
            float shallowest = 1e9F;
            for (std::size_t i = 0; i < m.flags.size(); ++i) {
                if ((m.flags[i] & kBridge) != 0) {
                    ++carved;
                    shallowest = std::min(shallowest, m.level[i] - m.bed[i]);
                }
            }
            CHECK(carved > 10);
            CHECK(shallowest >= min_depth - 1e-3F);
            CHECK(shallowest <= min_depth + 1e-3F);
        }
        // A wider brush carves more.
        RiverConfig wide;
        wide.bridge_half_width = 2;
        std::size_t narrow_n = 0, wide_n = 0;
        for (const std::uint8_t f :
             RiverMask::from_island(iw, pieces, covers, RiverConfig{}).flags) {
            narrow_n += (f & kBridge) != 0;
        }
        for (const std::uint8_t f : RiverMask::from_island(iw, pieces, covers, wide).flags) {
            wide_n += (f & kBridge) != 0;
        }
        CHECK(wide_n > narrow_n);
    }
}

TEST_CASE("river mask: sprite projection and coverage (the pure steps of Scene::river_mask)") {
    namespace detail = river::detail;
    // The coverage window is half the cell's projection: 5 x 3 pixels for the
    // original's 84 x 42 px tile at 8 cells per tile.
    CHECK(detail::half_window(84.0F, 8) == 2);
    CHECK(detail::half_window(42.0F, 8) == 1);
    CHECK(detail::half_window(84.0F, 16) <= detail::half_window(84.0F, 8));
    CHECK(detail::half_window(84.0F, 4) >= detail::half_window(84.0F, 8));

    // Projection and the original's depth test: a flat 8 x 8 tile field of
    // terrain 10, with the right half at 200.
    constexpr std::size_t n = 8;
    std::vector<std::uint8_t> heights(n * n, 10);
    for (std::size_t y = 0; y < n; ++y) {
        for (std::size_t x = 4; x < n; ++x) {
            heights[y * n + x] = 200;
        }
    }
    const float half_w = 42.0F, half_h = 21.0F, lift = 2.0F;
    const auto tested = detail::project_cells(heights, n, n, 0, 0, 8, 8, 8, half_w, half_h, lift,
                                              true, 10.0F + 10.0F);
    const auto untested = detail::project_cells(heights, n, n, 0, 0, 8, 8, 8, half_w, half_h, lift,
                                                false, 10.0F + 10.0F);
    REQUIRE(tested.size() == 64U * 64U);
    std::size_t pass_left = 0, pass_right = 0, pass_all = 0;
    for (std::size_t j = 0; j < 64; ++j) {
        for (std::size_t i = 0; i < 64; ++i) {
            (i < 32 ? pass_left : pass_right) += tested[j * 64 + i].pass ? 1U : 0U;
            pass_all += untested[j * 64 + i].pass ? 1U : 0U;
        }
    }
    // Terrain 10 <= 20 (the corners 3 and 4 are joined by a ramp up to 200):
    // drawn; terrain 200 hides the sprite.
    CHECK(pass_left >= 22U * 64U);
    CHECK(pass_left <= 26U * 64U);
    CHECK(pass_right == 0);
    CHECK(pass_all == 64U * 64U); // falls below the sea surface are not tested
    // The forward mapping: cell (0, 0) centre is tile (1/16, 1/16) at height 10.
    CHECK(tested[0].sx == doctest::Approx((0.5F / 8.0F - 0.5F / 8.0F) * half_w));
    CHECK(tested[0].sy == doctest::Approx((1.0F / 8.0F) * half_h - lift * 10.0F));

    // Coverage: the alpha closed over 3 x 3 pixels must fill at least half of
    // the 5 x 3 window of a cell; a blocked cell is never covered.
    auto cover_with = [&](int px, int py, bool pass) {
        detail::AlphaPlane plane;
        plane.x0 = 0, plane.y0 = 0, plane.w = 40, plane.h = 20;
        plane.alpha.assign(40U * 20U, 0);
        plane.alpha[static_cast<std::size_t>(py * plane.w + px)] = 255;
        std::vector<detail::CellPixel> cells(1);
        cells[0].sx = 10.3F, cells[0].sy = 8.7F, cells[0].pass = pass; // pixel (10, 8)
        return detail::cover_from_alpha(cells, plane, 2, 1)[0];
    };
    CHECK(cover_with(10, 8, true) == 1);  // on the cell: the closing fills 9 of 15
    CHECK(cover_with(11, 8, true) == 1);  // one pixel aside: 9 of 15 still
    CHECK(cover_with(12, 8, true) == 0);  // at the window's edge: 6 of 15
    CHECK(cover_with(13, 8, true) == 0);  // just outside: 3 of 15
    CHECK(cover_with(10, 8, false) == 0); // depth test blocked
    // A full block is covered.
    detail::AlphaPlane full;
    full.x0 = 0, full.y0 = 0, full.w = 40, full.h = 20;
    full.alpha.assign(40U * 20U, 255);
    std::vector<detail::CellPixel> one(1);
    one[0].sx = 10.3F, one[0].sy = 8.7F;
    CHECK(detail::cover_from_alpha(one, full, 2, 1)[0] == 1);
    // plane_for holds every cell with its window and the closing.
    std::vector<detail::CellPixel> two(2);
    two[0].sx = -20.0F, two[0].sy = 5.0F, two[1].sx = 30.0F, two[1].sy = 50.0F;
    const detail::AlphaPlane box = detail::plane_for(two, 2, 1);
    CHECK(box.x0 <= -20 - 3);
    CHECK(box.x0 + box.w >= 30 + 3);
    CHECK(box.y0 <= 5 - 2);
    CHECK(box.y0 + box.h >= 50 + 2);
    CHECK(box.alpha.size() == static_cast<std::size_t>(box.w) * static_cast<std::size_t>(box.h));
}

TEST_CASE("river flow: rivers without sea contact are listed and can be held still (OQ-067)") {
    // A river with a sea face and one without, 5 tiles apart.
    Chan c(260, 128);
    c.water(8, 16, 200, 40, 6.0F);
    c.sea(200, 16, 204, 40);
    c.water(8, 80, 200, 104, 6.0F);
    RiverConfig cfg;
    cfg.log_fallback = false;
    const RiverFlow f = RiverFlow::build(c.m, cfg);
    REQUIRE(f.rivers().size() == 2);
    REQUIRE(f.stats().sea_less_components == 1);
    const river::RiverInfo &a = f.rivers()[0], &b = f.rivers()[1];
    CHECK_FALSE(a.sea_less);
    CHECK(b.sea_less);
    CHECK(a.cells == 192U * 24U);
    CHECK(a.x0 == 8);
    CHECK(a.x1 == 199);
    CHECK(a.y0 == 16);
    CHECK(a.y1 == 39);
    CHECK(b.y0 == 80);
    CHECK(b.y1 == 103);
    // The pseudo outlet of the sea-less river lies in it.
    CHECK(b.pseudo_cx >= b.x0);
    CHECK(b.pseudo_cx <= b.x1);
    CHECK(b.pseudo_cy >= b.y0);
    CHECK(b.pseudo_cy <= b.y1);
    for (const auto& q : f.cells()) {
        CHECK(((q.flags & RiverFlow::kSeaLess) != 0) == (q.cy >= 80));
    }
    // Held still: the sea-less river stops, the other one does not change.
    RiverConfig still = cfg;
    still.sea_less_speed_scale = 0.0F;
    const RiverFlow g = RiverFlow::build(c.m, still);
    for (std::size_t i = 0; i < f.cells().size(); ++i) {
        if (f.cells()[i].cy >= 80) {
            CHECK(g.cells()[i].ux == 0.0F);
            CHECK(g.cells()[i].uy == 0.0F);
            CHECK(std::hypot(f.cells()[i].ux, f.cells()[i].uy) > 0.05F);
        } else {
            CHECK(g.cells()[i].ux == f.cells()[i].ux);
        }
    }
}
