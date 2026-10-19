// Tests of the hydraulic extras of the river flow (docs/render/rivers.md 2.3 -
// 2.7, 17; stage S7): obstacles and wakes, the Froude number, the turbulence
// channel, the thalweg shift in bends, the Picard refinement of the Manning
// conveyance. Synthetic rivers only: no game files.
//
// Like river_flow_test.cpp every case paints a RiverMask by hand (a channel of
// wet cells, banks, sea cells as outlets) and checks a law of the model against
// an analytic reference: the potential flow around a cylinder (E15), the lee
// wake exp(-t / L), Fr = u / sqrt(g h), the discharge split of a braid under
// the Manning law (flux ~ K sqrt(slope)) against the linear one (flux ~ K slope),
// the cross-section mean of the thalweg shift.
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

#include <doctest/doctest.h>

#include <anno/render/iso.hpp>
#include <anno/render/rect.hpp>
#include <anno/render/river.hpp>
#include <anno/render/sea.hpp>

namespace river = anno::render::river;
using river::RiverConfig;
using river::RiverFlow;
using river::RiverMask;

namespace {

constexpr std::int32_t kC = 8; // cells per tile
constexpr double kPi = 3.14159265358979323846;

/// The model without the visual post-processing, so that laws hold exactly.
RiverConfig exact() {
    RiverConfig c;
    c.speed_floor = 0.0F;
    c.speed_cap = 1000.0F;
    c.smoothing_passes = 0;
    c.transition_passes = 0;
    c.log_fallback = false;
    c.wake = false;
    return c;
}

struct Chan {
    RiverMask m;
    Chan(std::int32_t nx, std::int32_t ny)
        : m(static_cast<std::uint32_t>(kC), 0, 0, static_cast<std::uint32_t>(nx),
            static_cast<std::uint32_t>(ny)) {}
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
    /// Marks the wet cells within `radius` cells of (cx, cy) as an obstacle.
    std::size_t disc(double cx, double cy, double radius) {
        std::size_t n = 0;
        for (std::int32_t y = 0; y < static_cast<std::int32_t>(m.ny()); ++y) {
            for (std::int32_t x = 0; x < static_cast<std::int32_t>(m.nx()); ++x) {
                const double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
                const std::size_t i = m.index(x, y);
                if (dx * dx + dy * dy <= radius * radius && (m.flags[i] & river::kWet) != 0) {
                    m.flags[i] = static_cast<std::uint8_t>(m.flags[i] | river::kHole);
                    ++n;
                }
            }
        }
        return n;
    }
    /// Records an object of the footprint [x, x + w) x [y, y + h) (tiles) whose
    /// hole cells are already marked, for the wake.
    void object(int x, int y, int w, int h, std::size_t cells) {
        river::ObstacleInfo o;
        o.object = {1502, x, y, w, h};
        o.radius = std::sqrt(static_cast<float>(w * h) / 3.14159265F);
        o.result = river::ObstacleInfo::Result::Hole;
        o.hole_cells = cells;
        m.obstacles.push_back(o);
    }
};

const RiverFlow::Cell& cell(const RiverFlow& f, std::int32_t cx, std::int32_t cy) {
    const std::size_t k = f.cell_at(cx, cy);
    REQUIRE(k != RiverFlow::npos);
    return f.cells()[k];
}

double speed_of(const RiverFlow::Cell& q) {
    return std::hypot(static_cast<double>(q.ux), static_cast<double>(q.uy));
}

doctest::Approx rel(double v, double eps) {
    return doctest::Approx(v).epsilon(eps).scale(0.0);
}

} // namespace

// --- obstacles (rivers.md 2.6, E15) ---

TEST_CASE("obstacles: the flow around a round obstacle follows potential flow far from it") {
    // A wide channel (24 x 80 tiles, depth 6 units, uniform lateral inflow) with a
    // disc of 1 tile radius a in the middle: far from it the solved velocity is
    // the potential flow around a cylinder in a stream U,
    //   u = U (1 - a^2 (x^2 - y^2) / r^4), v = -2 U a^2 x y / r^4     (E15)
    // (x along the stream, origin at the centre), within 15 % at 2, 3 and 4 radii.
    // U(x) is measured at the same column at 8 radii to either side, where the
    // disturbance is 1.6 %. The wall rubs the water (bank roughness) within a
    // fraction of a radius; the walls of the channel are 12 radii away.
    const std::int32_t w = 192, len = 640, x0 = 8, y0 = 16;
    Chan c(len + 40, w + 32);
    c.water(x0, y0, x0 + len, y0 + w, 6.0F);
    c.sea(x0 + len, y0, x0 + len + 4, y0 + w);
    const double a = kC; // 1 tile
    const double xc = x0 + 480.0, yc = y0 + w / 2.0;
    REQUIRE(c.disc(xc, yc, a) > 150);
    const RiverFlow f = RiverFlow::build(c.m, exact());
    REQUIRE(f.stats().components == 1);
    CHECK(f.stats().non_finite == 0);
    CHECK(f.stats().residual < 1e-4);
    auto u_at = [&](double x, double y) -> const RiverFlow::Cell& {
        return cell(f, static_cast<std::int32_t>(std::floor(x)),
                    static_cast<std::int32_t>(std::floor(y)));
    };
    std::size_t checked = 0;
    double worst = 0.0;
    for (const double r_a : {2.0, 3.0, 4.0}) {
        for (const double deg : {0.0, 45.0, 90.0, 135.0, 180.0, 225.0, 270.0, 315.0}) {
            const double th = deg * kPi / 180.0;
            const double px = xc + r_a * a * std::cos(th), py = yc + r_a * a * std::sin(th);
            // The free stream at this column, far from the disc.
            const double U = 0.5 * (u_at(px, yc - 8 * a).ux + u_at(px, yc + 8 * a).ux);
            REQUIRE(U > 0.0);
            const double x = px - xc, y = py - yc, r2 = x * x + y * y;
            const double r4 = r2 * r2;
            const double u_ideal = U * (1.0 - a * a * (x * x - y * y) / r4);
            const double v_ideal = -2.0 * U * a * a * x * y / r4;
            const RiverFlow::Cell& q = u_at(px, py);
            // Relative to the ideal vector (its length is never below 0.7 U here).
            const double err = std::hypot(static_cast<double>(q.ux) - u_ideal,
                                          static_cast<double>(q.uy) - v_ideal) /
                               std::hypot(u_ideal, v_ideal);
            worst = std::max(worst, err);
            ++checked;
            INFO("r = " << r_a << " a, angle " << deg << ": u/U " << q.ux / U << " (ideal "
                        << u_ideal / U << "), v/U " << q.uy / U << " (ideal " << v_ideal / U
                        << ")");
            CHECK(err < 0.15);
        }
    }
    CHECK(checked == 24);
    MESSAGE("worst relative error of the far field: " << worst);
    // The obstacle matters (a field without it differs from the ideal by more): the
    // stagnation point upstream is slower than the stream, the shoulders faster.
    const double U = 0.5 * (u_at(xc, yc - 8 * a).ux + u_at(xc, yc + 8 * a).ux);
    CHECK(u_at(xc - 2 * a, yc).ux < 0.85 * U);
    CHECK(u_at(xc, yc + 2 * a).ux > 1.12 * U);
    CHECK(u_at(xc, yc - 2 * a).ux > 1.12 * U);
}

TEST_CASE(
    "obstacles: no flow in a hole, the discharge goes around it, a hole is no river and no pond") {
    // A channel 8 tiles wide with a hole in the middle: u = 0 inside (and the cells
    // are flagged), the flux through the section that holds the hole is still the
    // inflow upstream of it (continuity: the water goes around), the speed in the
    // free gap beside it exceeds the free stream, and the hole is neither a
    // river nor a pond of its own.
    const std::int32_t w = 64, len = 320, x0 = 8, y0 = 16;
    Chan c(len + 40, w + 32);
    c.water(x0, y0, x0 + len, y0 + w, 6.0F);
    c.sea(x0 + len, y0, x0 + len + 4, y0 + w);
    const std::size_t holes = c.disc(x0 + 200.0, y0 + w / 2.0, 12.0);
    REQUIRE(holes > 400);
    const RiverFlow f = RiverFlow::build(c.m, exact());
    CHECK(f.stats().hole_cells == holes);
    CHECK(f.stats().components == 1);
    CHECK(f.stats().ponds == 0);
    CHECK(f.stats().wet_cells == static_cast<std::size_t>(len * w));
    CHECK(f.stats().residual < 1e-4);
    std::size_t flagged = 0;
    for (const auto& q : f.cells()) {
        const bool hole = (c.m.flags[c.m.index(q.cx, q.cy)] & river::kHole) != 0;
        CHECK(((q.flags & RiverFlow::kHoleCell) != 0) == hole);
        if (hole) {
            ++flagged;
            CHECK(q.ux == 0.0F);
            CHECK(q.uy == 0.0F);
            CHECK(q.turb == 0.0F);
        }
    }
    CHECK(flagged == holes);
    // Continuity: the flux through a section equals the lateral inflow upstream
    // of it (the area of the water upstream, without the hole's cells), through the
    // section of the hole and the clear ones before and after it.
    auto flux = [&](std::int32_t cx) {
        double q = 0.0;
        for (const auto& s : f.cells()) {
            if (s.cx == cx) {
                q += static_cast<double>(s.ux) * static_cast<double>(s.depth) / kC;
            }
        }
        return q;
    };
    auto area_upstream = [&](std::int32_t cx) {
        double a = 0.0;
        for (const auto& s : f.cells()) {
            a += (s.cx <= cx && (s.flags & RiverFlow::kHoleCell) == 0) ? 1.0 : 0.0;
        }
        return a;
    };
    const double per_cell = flux(x0 + len - 1) / area_upstream(x0 + len - 1);
    for (const std::int32_t cx : {x0 + 120, x0 + 200, x0 + 280}) {
        CHECK(flux(cx) == rel(per_cell * area_upstream(cx), 0.03));
    }
    // Speed-up beside the hole and stagnation upstream of it, against the same
    // channel without it.
    Chan open(len + 40, w + 32);
    open.water(x0, y0, x0 + len, y0 + w, 6.0F);
    open.sea(x0 + len, y0, x0 + len + 4, y0 + w);
    const RiverFlow g = RiverFlow::build(open.m, exact());
    const std::int32_t xh = x0 + 200, yh = y0 + w / 2;
    const double beside = cell(f, xh, yh + 16).ux, beside_free = cell(g, xh, yh + 16).ux;
    CHECK(beside > 1.15 * beside_free);
    const double before = cell(f, xh - 16, yh).ux, before_free = cell(g, xh - 16, yh).ux;
    CHECK(before < 0.85 * before_free);
    // Far from it nothing changes: upstream of the hole by 12 tiles the two fields agree.
    CHECK(cell(f, xh - 100, yh).ux == rel(cell(g, xh - 100, yh).ux, 0.03));
}

TEST_CASE(
    "obstacles: the wake behind an object decays as exp(-t / L) with L = wake_length x radius") {
    // A 2 x 2 tile object (a_eff = 1.128 tiles, L = 9.03 tiles at wake_length 8) in a
    // channel 12 tiles wide: behind it (downstream) the wake is exp(-t / L) at the
    // distance t from the hole, nothing in front of it or far to the side, the
    // speed there is slowed by 1 - wake_slow * wake, and the option turns it off.
    const std::int32_t w = 96, len = 480, x0 = 8, y0 = 16;
    Chan c(len + 40, w + 32);
    c.water(x0, y0, x0 + len, y0 + w, 6.0F);
    c.sea(x0 + len, y0, x0 + len + 4, y0 + w);
    // The object 2 x 2 tiles at tile (xo, yo): hole = the footprint shrunk by 15 %.
    const int xo = 30, yo = 6;
    const double hcx = (xo + 1.0) * kC, hcy = (yo + 1.0) * kC; // centre in cells
    std::size_t holes = 0;
    for (std::int32_t y = 0; y < static_cast<std::int32_t>(c.m.ny()); ++y) {
        for (std::int32_t x = 0; x < static_cast<std::int32_t>(c.m.nx()); ++x) {
            const std::size_t i = c.m.index(x, y);
            if ((c.m.flags[i] & river::kWet) != 0 && std::fabs(x + 0.5 - hcx) <= 0.85 * kC &&
                std::fabs(y + 0.5 - hcy) <= 0.85 * kC) {
                c.m.flags[i] = static_cast<std::uint8_t>(c.m.flags[i] | river::kHole);
                ++holes;
            }
        }
    }
    REQUIRE(holes == 14 * 14);
    c.object(xo, yo, 2, 2, holes);
    RiverConfig cfg = exact();
    cfg.wake = true;
    const RiverFlow f = RiverFlow::build(c.m, cfg);
    cfg.wake = false;
    const RiverFlow plain = RiverFlow::build(c.m, cfg);
    CHECK(f.stats().wake_cells > 500);
    CHECK(plain.stats().wake_cells == 0);
    const double radius = std::sqrt(4.0 / kPi);
    const double length = 8.0 * radius; // tiles
    const auto cy = static_cast<std::int32_t>(hcy);
    const double edge = hcx + 0.85 * kC; // the hole's downstream edge, in cells
    for (const double t : {1.0, 3.0, 6.0}) {
        const auto cx = static_cast<std::int32_t>(edge + t * kC);
        const auto& q = cell(f, cx, cy);
        // The distance from the hole's edge to the cell's centre (path of the trace).
        const double d = (cx + 0.5 - edge) / kC;
        INFO("t = " << d << " tiles: wake " << q.wake << ", expected " << std::exp(-d / length));
        CHECK(q.wake == rel(std::exp(-d / length), 0.08));
        // Slowed by exactly 1 - wake_slow * wake against the field without the wake.
        const auto& p = cell(plain, cx, cy);
        CHECK(q.ux ==
              rel(static_cast<double>(p.ux) * (1.0 - static_cast<double>(cfg.wake_slow) * q.wake),
                  1e-4));
        CHECK(p.wake == 0.0F);
        // It raises the turbulence (E16: the wake is one of its terms).
        CHECK(q.turb >= std::min(1.0F, q.wake) - 1e-6F);
    }
    // Nothing upstream, nothing 4 tiles to the side, nothing far downstream.
    CHECK(cell(f, static_cast<std::int32_t>(hcx - 2.0 * kC), cy).wake == 0.0F);
    CHECK(cell(f, static_cast<std::int32_t>(edge + 3.0 * kC), cy + 4 * kC).wake == 0.0F);
    CHECK(cell(f, static_cast<std::int32_t>(edge + (length + 2.0) * kC), cy).wake == 0.0F);
    // The length follows wake_length: halved, the wake at 3 tiles is exp(-3 / (L / 2)).
    cfg.wake = true;
    cfg.wake_length = 4.0F;
    const RiverFlow g = RiverFlow::build(c.m, cfg);
    const auto cx3 = static_cast<std::int32_t>(edge + 3.0 * kC);
    CHECK(cell(g, cx3, cy).wake ==
          rel(std::exp(-((cx3 + 0.5 - edge) / kC) / (length / 2.0)), 0.08));
    // The slow-down is the option: wake_slow 0 leaves the speeds alone.
    cfg.wake_slow = 0.0F;
    const RiverFlow h = RiverFlow::build(c.m, cfg);
    CHECK(cell(h, cx3, cy).ux == rel(cell(plain, cx3, cy).ux, 1e-6));
    CHECK(cell(h, cx3, cy).wake > 0.1F);
}

TEST_CASE("Froude number: Fr = u / sqrt(g h), supercritical only where the water falls") {
    // The channel of the fall test: level 64 -> 16 over 3 tiles, 3 units of water over
    // the fall and 12 units on the flat reaches.
    Chan c(360, 64);
    for (std::int32_t x = 8; x < 328; ++x) {
        const double t = std::clamp((x - 120.0) / 24.0, 0.0, 1.0);
        const float level = static_cast<float>(64.0 - 48.0 * t);
        c.water(x, 16, x + 1, 40, t > 0.0 && t < 1.0 ? 3.0F : 12.0F, level);
    }
    c.sea(328, 16, 332, 40);
    RiverConfig cfg = exact();
    cfg.speed_cap = 3.0F;
    const RiverFlow f = RiverFlow::build(c.m, cfg);
    std::size_t fall = 0, calm = 0;
    for (const auto& q : f.cells()) {
        const double sp = speed_of(q);
        // The law, per cell, with the cell's own depth (tiles).
        CHECK(static_cast<double>(q.froude) ==
              rel(sp / std::sqrt(static_cast<double>(cfg.gravity) * static_cast<double>(q.depth)),
                  1e-4));
        if (q.slope >= anno::render::kRapidsFull) {
            ++fall;
            CHECK(q.froude > 1.0F); // 2.5 tiles/s over 3 units of water: Fr 1.9
        } else if (q.slope < 1e-3F && std::abs(q.cx - 132) > 40) {
            ++calm;
            CHECK(q.froude < 0.6F);
        }
    }
    CHECK(fall > 100);
    CHECK(calm > 1000);
    // The constant is the wave simulation's: deep_speed^2 / (depth_cap * 0.04 tiles).
    CHECK(cfg.gravity == doctest::Approx(3.0F * 3.0F / (16.0F * anno::render::kHeightUnitTiles)));
}

TEST_CASE("turbulence: the bank shear layer, and ponds and holes are calm") {
    // A straight channel 6 tiles wide, 12 units deep (Fr 0.35: no rapids): the
    // turbulence is the bank shear layer alone, 0.35 smoothstep(0.4, 0, d) of the
    // distance d to the bank, and nothing in the middle.
    Chan c(360, 96);
    c.water(8, 24, 328, 24 + 48, 12.0F);
    c.sea(328, 24, 332, 24 + 48);
    RiverConfig cfg = exact();
    const RiverFlow f = RiverFlow::build(c.m, cfg);
    for (const std::int32_t row : {0, 1, 2, 3, 4, 5, 8, 24}) {
        const auto& q = cell(f, 200, 24 + row);
        // The distance of the cell to the bank: the chamfer distance in cells (row + 1)
        // less half a cell, times the cell size.
        const double d = (row + 1.0 - 0.5) / kC;
        const double e = 0.35 * anno::render::smoothstep(0.4F, 0.0F, static_cast<float>(d));
        INFO("row " << row << ": d " << d << " turb " << q.turb << " expected " << e);
        // 1 % of the layer's strength (the chamfer distance of a cell a few cells from
        // the bank is a few percent of a cell off, which the steep end of the
        // smoothstep turns into a few 1e-4).
        CHECK(std::fabs(static_cast<double>(q.turb) - e) < 0.01 + 0.02 * e);
    }
    CHECK(cell(f, 200, 24 + 24).turb < 1e-3F);
    // The weights are the config's.
    cfg.turb_bank = 0.0F;
    CHECK(cell(RiverFlow::build(c.m, cfg), 200, 24).turb < 1e-3F);
    cfg.turb_bank = 0.7F;
    CHECK(cell(RiverFlow::build(c.m, cfg), 200, 24).turb ==
          rel(0.7 * anno::render::smoothstep(0.4F, 0.0F, 0.5F / kC), 0.01));
    // Everything stays within 0..1, ponds are calm.
    cfg = exact();
    Chan d(360, 96);
    d.water(8, 24, 328, 24 + 48, 12.0F);
    d.water(100, 4, 103, 7, 4.0F); // a pond (3 x 3 cells: below pond_cells)
    d.sea(328, 24, 332, 24 + 48);
    const RiverFlow g = RiverFlow::build(d.m, cfg);
    CHECK(g.stats().ponds == 1);
    std::size_t ponds = 0;
    for (const auto& q : g.cells()) {
        CHECK(q.turb >= 0.0F);
        CHECK(q.turb <= 1.0F);
        if (q.river == RiverFlow::kPond) {
            CHECK(q.turb == 0.0F);
            ++ponds;
        }
    }
    CHECK(ponds == 9);
}

TEST_CASE(
    "turbulence: a bend and a fall raise it where the direction changes and where Fr is high") {
    // The 90 degree channel of the bend test (8 tiles wide): the flow turns by about
    // 0.4 radians within one tile in the middle of the corner (0.5 per radian) and not
    // on the legs; on a fall (Fr 1.9) the rapids term is 0.5.
    Chan c(240, 240);
    c.water(8, 8, 200, 8 + 64, 12.0F);
    c.water(136, 8, 200, 208, 12.0F);
    c.sea(136, 208, 200, 212);
    RiverConfig cfg = exact();
    const RiverFlow f = RiverFlow::build(c.m, cfg);
    // Middle of the channel (outside the bank layer): leg 1 against the corner.
    double leg = 0.0, corner = 0.0;
    for (std::int32_t y = 8 + 24; y < 8 + 40; ++y) {
        leg = std::max(leg, static_cast<double>(cell(f, 60, y).turb));
    }
    for (std::int32_t x = 136 + 16; x < 200 - 16; ++x) {
        for (std::int32_t y = 8 + 16; y < 8 + 48; ++y) {
            corner = std::max(corner, static_cast<double>(cell(f, x, y).turb));
        }
    }
    CHECK(leg < 0.02);
    CHECK(corner > 0.1);
    // The bend term is 0.5 per radian: cut it and the corner is as calm as the leg.
    cfg.turb_bend = 0.0F;
    const RiverFlow g = RiverFlow::build(c.m, cfg);
    double corner0 = 0.0;
    for (std::int32_t x = 136 + 16; x < 200 - 16; ++x) {
        for (std::int32_t y = 8 + 16; y < 8 + 48; ++y) {
            corner0 = std::max(corner0, static_cast<double>(cell(g, x, y).turb));
        }
    }
    CHECK(corner0 < 0.02);
    // A fall: the rapids term (0.5 smoothstep(0.45, 0.95, Fr)), full at Fr 1.9.
    Chan d(360, 64);
    for (std::int32_t x = 8; x < 328; ++x) {
        const double t = std::clamp((x - 120.0) / 24.0, 0.0, 1.0);
        d.water(x, 16, x + 1, 40, t > 0.0 && t < 1.0 ? 3.0F : 12.0F,
                static_cast<float>(64.0 - 48.0 * t));
    }
    d.sea(328, 16, 332, 40);
    RiverConfig dcfg = exact();
    const RiverFlow h = RiverFlow::build(d.m, dcfg);
    const auto& mid = cell(h, 132, 28); // the middle of the fall, away from the banks
    CHECK(mid.froude > 1.0F);
    CHECK(mid.turb >= 0.49F);
    dcfg.turb_rapids = 0.0F;
    CHECK(cell(RiverFlow::build(d.m, dcfg), 132, 28).turb < 0.2F);
}

namespace {

/// A quarter-circle bend of a channel (the water between the radii ri and ro cells
/// around (cx, cy), in the quadrant x >= cx, y >= cy), with a straight leg of `leg`
/// cells on either end: the flow comes down (+y) leg A, turns through the arc and
/// leaves along -x through leg B to the sea.
struct Bend {
    Chan c;
    double cx, cy, ri, ro;
    Bend(double ri_, double ro_, std::int32_t leg)
        : c(static_cast<std::int32_t>(ro_) + leg + 80, static_cast<std::int32_t>(ro_) + leg + 80),
          cx(leg + 40.0), cy(leg + 40.0), ri(ri_), ro(ro_) {
        const auto icx = static_cast<std::int32_t>(cx), icy = static_cast<std::int32_t>(cy);
        for (std::int32_t y = 0; y < static_cast<std::int32_t>(c.m.ny()); ++y) {
            for (std::int32_t x = 0; x < static_cast<std::int32_t>(c.m.nx()); ++x) {
                const double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
                const double r = std::hypot(dx, dy);
                const bool arc = dx >= 0 && dy >= 0 && r >= ri && r < ro;
                const bool leg_a = dx >= ri && dx < ro && y >= icy - leg && y < icy;
                const bool leg_b = dy >= ri && dy < ro && x >= icx - leg && x < icx;
                if (arc || leg_a || leg_b) {
                    c.m.set_wet(x, y, 10.0F - 12.0F, 10.0F);
                }
            }
        }
        // The sea beyond the end of leg B.
        for (std::int32_t y = 0; y < static_cast<std::int32_t>(c.m.ny()); ++y) {
            for (std::int32_t x = icx - leg - 4; x < icx - leg; ++x) {
                const double dy = y + 0.5 - cy;
                if (dy >= ri && dy < ro) {
                    c.m.set_sea(x, y);
                }
            }
        }
    }
    /// The speed along the radial line at `degrees` (0 = where leg A joins, 90 = leg B),
    /// inner bank to outer bank (one entry per cell of radius), smoothed over three
    /// neighbours.
    [[nodiscard]] std::vector<double> profile(const RiverFlow& f, double degrees = 45.0) const {
        std::vector<double> v;
        const double ca = std::cos(degrees * kPi / 180.0), sa = std::sin(degrees * kPi / 180.0);
        for (double r = ri + 0.5; r < ro - 0.5; r += 1.0) {
            const double x = cx + r * ca, y = cy + r * sa;
            v.push_back(speed_of(cell(f, static_cast<std::int32_t>(std::floor(x)),
                                      static_cast<std::int32_t>(std::floor(y)))));
        }
        std::vector<double> s(v.size());
        for (std::size_t i = 0; i < v.size(); ++i) {
            const std::size_t a = i == 0 ? 0 : i - 1, b = std::min(i + 1, v.size() - 1);
            s[i] = (v[a] + v[i] + v[b]) / 3.0;
        }
        return s;
    }
};

double peak_position(const std::vector<double>& s) {
    const auto it = std::max_element(s.begin(), s.end());
    return static_cast<double>(it - s.begin()) / static_cast<double>(s.size() - 1);
}

double mean_of(const std::vector<double>& s) {
    double sum = 0.0;
    for (const double x : s) {
        sum += x;
    }
    return sum / static_cast<double>(s.size());
}

} // namespace

TEST_CASE("thalweg shift: potential flow is fastest at the inner bank of a bend, the shift moves "
          "the speed outward and keeps the cross-section mean") {
    // A quarter-circle channel 4 tiles wide (radii 6 and 10 tiles): the irrotational
    // flow of the model is faster on the inner side of a bend (rivers.md 2.5, the
    // opposite of nature). E14 multiplies the speed by 1 + beta tanh(|kappa| W) n_hat
    // (n_hat: -1 at the inner bank to +1 at the outer one), renormalised over the
    // lateral section. The tilt of a section is the mean speed of its outer core
    // (0.6 to 0.8 of the way from the inner bank) over that of its inner core (0.2 to
    // 0.4); the banks themselves are slow (bank roughness) and are left out.
    // beta = 0.35 evens the sides out by about two thirds of the potential flow's
    // tilt but does not reverse it (the slope of its factor, at most 2 beta / W, is
    // less than the 1 / r of the vortex): rivers.md 17.5, OQ-080; beta 0.9 reverses it.
    const Bend b(48.0, 80.0, 160);
    auto tilt = [](const std::vector<double>& s) {
        const std::size_t n = s.size();
        auto mean = [&](double lo, double hi) {
            double sum = 0.0;
            std::size_t k = 0;
            for (auto i = static_cast<std::size_t>(lo * static_cast<double>(n - 1));
                 i <= static_cast<std::size_t>(hi * static_cast<double>(n - 1)); ++i, ++k) {
                sum += s[i];
            }
            return sum / static_cast<double>(k);
        };
        return mean(0.6, 0.8) / mean(0.2, 0.4);
    };
    RiverConfig cfg = exact();
    const RiverFlow off = RiverFlow::build(b.c.m, cfg);
    REQUIRE(off.stats().components == 1);
    const std::vector<double> p_off = b.profile(off);
    REQUIRE(p_off.size() > 28);
    // Off: the inner core is faster than the outer one.
    CHECK(off.stats().thalweg_cells == 0);
    cfg.thalweg_shift = true;
    const RiverFlow on = RiverFlow::build(b.c.m, cfg);
    CHECK(on.stats().thalweg_cells > 1000);
    const std::vector<double> p_on = b.profile(on);
    cfg.thalweg_beta = 0.9F;
    const RiverFlow strong = RiverFlow::build(b.c.m, cfg);
    const std::vector<double> p_strong = b.profile(strong);
    MESSAGE("tilt (outer core / inner core) at 45 degrees: off "
            << tilt(p_off) << ", beta 0.35 " << tilt(p_on) << ", beta 0.9 " << tilt(p_strong)
            << "; position of the maximum " << peak_position(p_off) << ", " << peak_position(p_on)
            << ", " << peak_position(p_strong));
    CHECK(tilt(p_off) < 0.9);
    // On (beta 0.35): the outer part gains and the inner part loses, the tilt grows.
    CHECK(tilt(p_on) > tilt(p_off) + 0.07);
    CHECK(tilt(p_on) < 1.0); // does not reverse (documented limitation)
    // beta 0.9: reversed, the outer core is faster, and the maximum has moved outward.
    CHECK(tilt(p_strong) > 1.05);
    CHECK(peak_position(p_strong) > peak_position(p_off) + 0.1);
    // The mean speed over the section is unchanged (the renormalisation of E14).
    CHECK(mean_of(p_on) == rel(mean_of(p_off), 0.04));
    CHECK(mean_of(p_strong) == rel(mean_of(p_off), 0.08));
    // beta 0 is the option off.
    cfg.thalweg_beta = 0.0F;
    const RiverFlow zero = RiverFlow::build(b.c.m, cfg);
    CHECK(zero.stats().thalweg_cells == 0);
    // Deterministic.
    cfg.thalweg_beta = 0.35F;
    const RiverFlow again = RiverFlow::build(b.c.m, cfg);
    CHECK(b.profile(again) == p_on);
    // A straight channel has no curvature: the shift leaves it (nearly) alone.
    Chan s(360, 96);
    s.water(8, 24, 328, 24 + 48, 8.0F);
    s.sea(328, 24, 332, 24 + 48);
    RiverConfig scfg = exact();
    const RiverFlow s_off = RiverFlow::build(s.m, scfg);
    scfg.thalweg_shift = true;
    const RiverFlow s_on = RiverFlow::build(s.m, scfg);
    for (const std::int32_t row : {2, 12, 24, 36}) {
        CHECK(cell(s_on, 200, 24 + row).ux == rel(cell(s_off, 200, 24 + row).ux, 1e-3));
    }
}

TEST_CASE("Picard refinement: the Manning law splits a braid by sqrt(L), the linear one by L, and "
          "it is guarded") {
    // A wide trunk feeds two straight arms of the same section that end at the sea,
    // one twice as long as the other. The same drop over lengths L and 2 L: with
    // the linear conveyance the flux of an arm is K / L (ratio 2), with the Manning
    // law K sqrt(slope) (flux ~ 1 / sqrt(L): ratio sqrt(2)) (rivers.md 2.3).
    Chan c(8 + 80 + 128 + 60, 16 + 160 + 16);
    c.water(8, 16, 88, 16 + 160, 8.0F);
    const std::int32_t ya = 16 + 40, yb = 16 + 110;
    c.water(88, ya, 88 + 64, ya + 3, 8.0F);
    c.sea(88 + 64, ya, 88 + 68, ya + 3);
    c.water(88, yb, 88 + 128, yb + 3, 8.0F);
    c.sea(88 + 128, yb, 88 + 132, yb + 3);
    auto arm_flux = [&](const RiverFlow& f, std::int32_t cx, std::int32_t y0) {
        double q = 0.0;
        for (std::int32_t y = y0; y < y0 + 3; ++y) {
            const auto& s = cell(f, cx, y);
            q += static_cast<double>(s.ux) * static_cast<double>(s.depth);
        }
        return q;
    };
    RiverConfig cfg = exact();
    const RiverFlow lin = RiverFlow::build(c.m, cfg);
    REQUIRE(lin.stats().components == 1);
    CHECK(lin.stats().picard_rivers == 0);
    const double ratio_lin = arm_flux(lin, 88 + 62, ya) / arm_flux(lin, 88 + 126, yb);
    cfg.picard_iterations = 12;
    const RiverFlow pic = RiverFlow::build(c.m, cfg);
    CHECK(pic.stats().picard_rivers == 1);
    CHECK(pic.stats().picard_fallbacks == 0);
    CHECK(pic.stats().picard_steps >= 2);
    CHECK(pic.stats().picard_steps <= 12);
    CHECK(pic.stats().flux_imbalance < 1e-3);
    CHECK(pic.stats().residual < 1e-3);
    const double ratio_pic = arm_flux(pic, 88 + 62, ya) / arm_flux(pic, 88 + 126, yb);
    MESSAGE("flux ratio short / long arm: linear " << ratio_lin << ", Picard " << ratio_pic
                                                   << " (2 and sqrt 2 in the ideal case)");
    CHECK(ratio_lin == rel(2.0, 0.08));
    CHECK(ratio_pic == rel(std::sqrt(2.0), 0.08));
    CHECK(ratio_pic < 0.85 * ratio_lin);
    // The discharge is the same model's: the speeds are finite and the field has no NaN.
    CHECK(pic.stats().non_finite == 0);
    // Guard: an iteration that cannot converge in its cap leaves the linear solution,
    // counted, and the result equals the linear one cell for cell.
    cfg.picard_iterations = 1;
    cfg.picard_tolerance = 1e-12F;
    const RiverFlow stuck = RiverFlow::build(c.m, cfg);
    CHECK(stuck.stats().picard_rivers == 1);
    CHECK(stuck.stats().picard_fallbacks == 1);
    REQUIRE(stuck.cells().size() == lin.cells().size());
    bool same = true;
    for (std::size_t i = 0; i < lin.cells().size(); ++i) {
        same = same && stuck.cells()[i].ux == lin.cells()[i].ux &&
               stuck.cells()[i].uy == lin.cells()[i].uy;
    }
    CHECK(same);
    // Deterministic.
    cfg.picard_iterations = 12;
    cfg.picard_tolerance = 1e-3F;
    const RiverFlow pic2 = RiverFlow::build(c.m, cfg);
    CHECK(cell(pic2, 88 + 62, ya).ux == cell(pic, 88 + 62, ya).ux);
}

namespace {

/// An island with a channel `k` corners wide (water over the tiles between) along y
/// from tile 2 to tile 21, corners x = 8 .. 8 + k - 1, banks 16 units up.
anno::render::sea::IslandWater channel_island(std::size_t k) {
    namespace sea = anno::render::sea;
    constexpr std::size_t n = 28;
    sea::IslandWater iw;
    iw.w = iw.h = n;
    iw.heights.assign(n * n, 40);
    for (std::size_t y = 0; y < n; ++y) {
        for (std::size_t x = 8; x < 8 + k; ++x) {
            iw.heights[y * n + x] = 24;
        }
    }
    iw.sea_chunk.assign(16, false);
    iw.open.assign(n * n, 0.0F);
    iw.shore.assign(n * n, 0.0F);
    iw.river.assign(n * n, std::numeric_limits<float>::quiet_NaN());
    for (std::size_t y = 2; y <= 21; ++y) {
        for (std::size_t x = 7; x <= 8 + k; ++x) {
            iw.river[y * n + x] = 24.0F + sea::kRiverLevel;
        }
    }
    return iw;
}

} // namespace

TEST_CASE("obstacles: which objects become holes (footprint, wet share, shrink, dams)") {
    // Channel 5 corners wide: tiles 8..11 between the low corners are water, the
    // rock-sized footprints below sit in it.
    const auto iw = channel_island(5);
    const std::vector<anno::render::sea::RiverPiece> pieces = {{7, 2, 6, 20, 24}};
    RiverConfig cfg;
    using Result = river::ObstacleInfo::Result;
    auto build = [&](const std::vector<river::Obstacle>& objects, const RiverConfig& k) {
        return RiverMask::from_island(iw, pieces, {}, k, objects);
    };
    // A 2 x 2 object fully in the water: the wet cells of the footprint shrunk by 15 %
    // about its centre, 14 x 14 of the 16 x 16.
    {
        const RiverMask m = build({{1502, 9, 10, 2, 2}}, cfg);
        REQUIRE(m.obstacles.size() == 1);
        const river::ObstacleInfo& o = m.obstacles[0];
        CHECK(o.result == Result::Hole);
        CHECK(o.wet_share == doctest::Approx(1.0F));
        CHECK(o.hole_cells == 14 * 14);
        CHECK(o.radius == doctest::Approx(std::sqrt(4.0F / 3.14159265F)).epsilon(1e-4));
        std::size_t holes = 0;
        for (const std::uint8_t f : m.flags) {
            holes += (f & river::kHole) != 0 ? 1U : 0U;
            if ((f & river::kHole) != 0) {
                CHECK((f & river::kWet) != 0); // a hole is a wet cell
            }
        }
        CHECK(holes == 14 * 14);
    }
    // The shrink is the config's: none, the whole 16 x 16; 50 %, 8 x 8.
    {
        RiverConfig k = cfg;
        k.obstacle_shrink = 0.0F;
        CHECK(build({{1502, 9, 10, 2, 2}}, k).obstacles[0].hole_cells == 16 * 16);
        k.obstacle_shrink = 0.5F;
        CHECK(build({{1502, 9, 10, 2, 2}}, k).obstacles[0].hole_cells == 8 * 8);
    }
    // Off: no holes, nothing recorded.
    {
        RiverConfig k = cfg;
        k.obstacles = false;
        const RiverMask m = build({{1502, 9, 10, 2, 2}}, k);
        CHECK(m.obstacles.empty());
        for (const std::uint8_t f : m.flags) {
            CHECK((f & river::kHole) == 0);
        }
    }
    // A footprint above the area limit (16 tiles) is left out, one at the limit is not.
    {
        const RiverMask m = build({{1553, 9, 8, 5, 4}, {1553, 9, 8, 4, 4}}, cfg);
        REQUIRE(m.obstacles.size() == 2);
        CHECK(m.obstacles[0].result == Result::TooBig);
        CHECK(m.obstacles[0].hole_cells == 0);
        CHECK(m.obstacles[1].result != Result::TooBig);
    }
    // Mostly on dry land: below the wet share (0.5) it is left out. Tiles 12 and 13
    // are bank (dry), a 2 x 2 object at x = 12 has 0 wet cells; one at x = 11 has
    // the half of it that lies in tile 11.
    {
        const RiverMask m = build({{1502, 13, 10, 2, 2}, {1502, 11, 10, 2, 2}}, cfg);
        REQUIRE(m.obstacles.size() == 2);
        CHECK(m.obstacles[0].result == Result::NotWet);
        CHECK(m.obstacles[0].wet_share < 0.5F);
        CHECK(m.obstacles[0].hole_cells == 0);
        CHECK(m.obstacles[1].wet_share == doctest::Approx(0.5F).epsilon(0.2));
    }
    // The share is a config value too: at 0.1 the object at the bank is used.
    {
        RiverConfig k = cfg;
        k.obstacle_min_wet = 0.1F;
        const RiverMask m = build({{1502, 11, 10, 2, 2}}, k);
        CHECK(m.obstacles[0].result == Result::Hole);
        CHECK(m.obstacles[0].hole_cells > 0);
    }
}

TEST_CASE(
    "obstacles: an object that would dam its river is left out, one that leaves a way is not") {
    // A channel 2 corners wide (one tile of water, 1.6 tiles with its soft edge, x from
    // 7.4 to 9.6): a 3 x 3 object at tile 7 stands across all of it, its hole would cut
    // the river in two. A 2 x 2 object at tile 8 leaves a way of 6 cells past it, which
    // is enough for the 3 cells of obstacle_gap_cells and not for 9. In the channel of
    // 5 corners the same object leaves a way past it.
    using Result = river::ObstacleInfo::Result;
    RiverConfig cfg;
    cfg.obstacle_min_wet = 0.3F; // the 3 x 3 footprint is wet only over its middle
    const auto narrow = channel_island(2);
    const std::vector<anno::render::sea::RiverPiece> narrow_pieces = {{7, 2, 3, 20, 24}};
    auto build = [&](const RiverConfig& k, river::Obstacle o) {
        return RiverMask::from_island(narrow, narrow_pieces, {}, k, {{o}});
    };
    {
        const RiverMask m = build(cfg, {1502, 7, 10, 3, 3});
        REQUIRE(m.obstacles.size() == 1);
        INFO("wet share " << m.obstacles[0].wet_share);
        CHECK(m.obstacles[0].result == Result::Dam);
        CHECK(m.obstacles[0].hole_cells == 0);
        for (const std::uint8_t f : m.flags) {
            CHECK((f & river::kHole) == 0);
        }
        // The same object at the end of the river leaves the water on one side only:
        // nothing is cut.
        const RiverMask e = build(cfg, {1502, 7, 1, 3, 3});
        INFO("end: " << static_cast<int>(e.obstacles[0].result));
        CHECK(e.obstacles[0].result == Result::Hole);
    }
    // A way of 6 cells: the default gap (3) lets it pass, a gap of 9 closes it.
    {
        const RiverMask m = build(cfg, {1502, 8, 10, 2, 2});
        CHECK(m.obstacles[0].result == Result::Hole);
        CHECK(m.obstacles[0].hole_cells > 100);
        RiverConfig k = cfg;
        k.obstacle_gap_cells = 9;
        const RiverMask d = build(k, {1502, 8, 10, 2, 2});
        CHECK(d.obstacles[0].result == Result::Dam);
        k.obstacle_gap_cells = 0;
        CHECK(build(k, {1502, 7, 10, 3, 3}).obstacles[0].result == Result::Dam);
    }
    {
        const auto iw = channel_island(5);
        const std::vector<anno::render::sea::RiverPiece> pieces = {{7, 2, 6, 20, 24}};
        const RiverMask m = RiverMask::from_island(iw, pieces, {}, cfg, {{{1502, 9, 10, 2, 2}}});
        CHECK(m.obstacles[0].result == Result::Hole);
    }
    // Objects are taken in the order given: the second of two that together close the
    // channel is the one refused.
    {
        const auto iw = channel_island(3);
        const std::vector<anno::render::sea::RiverPiece> pieces = {{7, 2, 4, 20, 24}};
        const RiverMask m = RiverMask::from_island(iw, pieces, {}, cfg,
                                                   {{{1502, 7, 10, 2, 2}, {1503, 9, 10, 2, 2}}});
        REQUIRE(m.obstacles.size() == 2);
        INFO("first " << static_cast<int>(m.obstacles[0].result) << ", second "
                      << static_cast<int>(m.obstacles[1].result));
        CHECK(m.obstacles[0].result == Result::Hole);
        CHECK(m.obstacles[1].result == Result::Dam);
    }
}

TEST_CASE("river extras: the configuration refuses what the model cannot use") {
    RiverConfig base;
    CHECK(base.validate(512, 3).empty());
    auto refused = [&](auto&& edit) {
        RiverConfig c;
        edit(c);
        return !c.validate(512, 3).empty();
    };
    CHECK(refused([](RiverConfig& c) { c.gravity = 0.0F; }));
    CHECK(refused([](RiverConfig& c) { c.froude_to = c.froude_from; }));
    CHECK(refused([](RiverConfig& c) { c.turb_fr_to = c.turb_fr_from; }));
    CHECK(refused([](RiverConfig& c) { c.turb_bank_tiles = 0.0F; }));
    CHECK(refused([](RiverConfig& c) { c.aeration = 1.5F; }));
    CHECK(refused([](RiverConfig& c) { c.wake_slow = -0.1F; }));
    CHECK(refused([](RiverConfig& c) { c.wake_length = 0.0F; }));
    CHECK(refused([](RiverConfig& c) { c.obstacle_min_wet = 2.0F; }));
    CHECK(refused([](RiverConfig& c) { c.obstacle_shrink = 0.95F; }));
    CHECK(refused([](RiverConfig& c) { c.standing_to = c.standing_from; }));
    CHECK(refused([](RiverConfig& c) { c.standing_min = 0.0F; }));
    CHECK(refused([](RiverConfig& c) { c.standing_max = c.standing_min * 0.5F; }));
    CHECK(refused([](RiverConfig& c) { c.picard_iterations = 65; }));
    CHECK(refused([](RiverConfig& c) { c.picard_relax = 0.0F; }));
    CHECK(refused([](RiverConfig& c) { c.picard_floor = 0.0F; }));
    CHECK(refused([](RiverConfig& c) { c.thalweg_beta = 1.0F; }));
    // The defaults are the design's (rivers.md 2.5-2.8, section 12): standing waves,
    // the thalweg shift and the Picard refinement are off, obstacles and wake on.
    CHECK_FALSE(base.standing_waves);
    CHECK_FALSE(base.thalweg_shift);
    CHECK(base.picard_iterations == 0);
    CHECK(base.obstacles);
    CHECK(base.wake);
    CHECK(base.obstacle_max_area == 16);
    CHECK(base.obstacle_min_wet == 0.5F);
    CHECK(base.obstacle_shrink == 0.15F);
    CHECK(base.wake_length == 8.0F);
    CHECK(base.wake_slow == 0.6F);
    CHECK(base.thalweg_beta == 0.35F);
    CHECK(base.aeration > 0.0F);
    // The reviewed ids are the rocks: every id of the design's candidates that stands
    // in a river is in the list, vegetation and buildings are not.
    const std::vector<std::uint32_t> ids = river::default_obstacle_ids();
    auto has = [&](std::uint32_t id) { return std::find(ids.begin(), ids.end(), id) != ids.end(); };
    for (const std::uint32_t id : {1502U, 1503U, 1505U, 1506U, 1507U, 1508U, 1549U, 1550U, 1551U,
                                   1572U, 1573U, 1574U, 1575U, 1576U, 1577U, 1580U, 1581U}) {
        CHECK(has(id));
    }
    for (const std::uint32_t id : {1U, 2U, 105U, 214U, 2536U, 3956U, 3961U, 3974U}) {
        CHECK_FALSE(has(id));
    }
}

// --- shading of the extras (rivers.md 2.5, 2.6, 2.8; the Froude rule, aeration, wake,
// standing waves, the ripple of objects in a river) ---

namespace {

namespace render = anno::render;

render::RiverView view0() {
    return {84.0F, 42.0F, 2.059F};
}

render::FlowSample flow_of(std::array<float, 2> u, float turb = 0.0F, float wake = 0.0F,
                           float shear = 0.0F) {
    render::FlowSample f;
    f.vx = u[0];
    f.vy = u[1];
    f.turb = turb;
    f.wake = wake;
    f.level = 32.0F;
    f.valid = 1.0F;
    f.shear = shear;
    return f;
}

render::Shade shade_at(const render::RiverParams& p, const render::FlowSample& f, float depth,
                       float slope, float px = 300.0F, float py = 900.0F, std::uint64_t t = 500,
                       float detail = 1.0F) {
    return render::river_flow_shade(px, py, f, depth, slope, t, detail, p, view0());
}

/// The Froude number of `river_flow_shade`: u / sqrt(g 0.04 max(depth, depth_min)).
double froude_of(const render::RiverParams& p, double speed, double depth) {
    return speed / std::sqrt(static_cast<double>(p.gravity) * 0.04 *
                             std::max(depth, static_cast<double>(p.depth_min)));
}

} // namespace

TEST_CASE(
    "river shading: the rapids follow the Froude number as well as the slope, aeration scales "
    "with them and is never white") {
    const river::RiverConfig cfg;
    const render::RiverParams p = cfg.params();
    // Flat water (slope 0): the aeration is aeration * smoothstep(froude_from,
    // froude_to, Fr) with the Fr of the speed over the depth (E12).
    for (const double depth : {2.0, 5.0, 12.0}) {
        float last = -1.0F;
        for (const float speed : {0.05F, 0.2F, 0.5F, 0.9F, 1.4F, 2.0F, 3.0F}) {
            const render::Shade s =
                shade_at(p, flow_of({speed, 0.0F}), static_cast<float>(depth), 0.0F);
            const double fr = froude_of(p, speed, depth);
            const double expect = static_cast<double>(p.aeration) *
                                  static_cast<double>(render::smoothstep(p.froude_from, p.froude_to,
                                                                         static_cast<float>(fr)));
            INFO("depth " << depth << " speed " << speed << " Fr " << fr);
            CHECK(static_cast<double>(s.aeration) ==
                  doctest::Approx(expect).epsilon(1e-4).scale(1.0));
            CHECK(s.aeration >= last - 1e-6F); // monotone in the speed
            CHECK(s.aeration <= p.aeration + 1e-6F);
            CHECK(s.foam == 0.0F); // no foam on rivers (owner, 2026-10-18)
            last = s.aeration;
        }
    }
    // Shallower water at the same speed is nearer to critical.
    CHECK(shade_at(p, flow_of({1.0F, 0.0F}), 2.0F, 0.0F).aeration >
          shade_at(p, flow_of({1.0F, 0.0F}), 12.0F, 0.0F).aeration);
    // The depth of the Froude number is at least depth_min (a river's edge has 0).
    CHECK(shade_at(p, flow_of({1.0F, 0.0F}), 0.0F, 0.0F).aeration ==
          shade_at(p, flow_of({1.0F, 0.0F}), p.depth_min, 0.0F).aeration);
    // The slope rule still makes rapids where the flow is slow (the painted falls):
    // the larger of the two rules.
    CHECK(shade_at(p, flow_of({0.1F, 0.0F}), 5.0F, 16.0F).aeration == doctest::Approx(p.aeration));
    CHECK(shade_at(p, flow_of({0.1F, 0.0F}), 5.0F, 0.0F).aeration == 0.0F);
    // Aeration tints toward kAerationColour and stays below the white of the foam:
    // at full aeration a pixel is the aeration colour (before the light), not white.
    const render::Px water{40, 110, 140, 255};
    render::Shade full;
    full.aeration = 1.0F;
    const render::Px out = render::shade_apply(water, full);
    for (std::size_t k = 0; k < 3; ++k) {
        CHECK(static_cast<float>(out[k]) ==
              doctest::Approx(render::kAerationColour[k] * 255.0F).epsilon(0.01));
    }
    CHECK(out[2] < 250); // the white of the foam is (236, 244, 250)
    CHECK(out[0] < 236);
    // Without aeration the pixel is what it was before the field existed.
    render::Shade plain;
    plain.light = 0.03F;
    plain.tint = 0.2F;
    const render::Px a = render::shade_apply(water, plain);
    const float shallow = render::kWaves.shallow_colour[0] * 255.0F;
    const float expect_r = (40.0F + (shallow - 40.0F) * 0.2F) * 1.03F;
    CHECK(std::fabs(static_cast<float>(a[0]) - expect_r) < 1.0F); // the bytes truncate
    // Aeration scaled by half is halfway to the colour (the order: tint, aeration, light).
    render::Shade half;
    half.aeration = 0.5F;
    const render::Px h = render::shade_apply(water, half);
    CHECK(static_cast<float>(h[1]) ==
          doctest::Approx(110.0F + (render::kAerationColour[1] * 255.0F - 110.0F) * 0.5F)
              .epsilon(0.01));
}

TEST_CASE("river shading: the lee of an obstacle is darker and its streaks stronger, turbulence "
          "raises the glints") {
    river::RiverConfig cfg;
    const render::RiverParams p = cfg.params();
    // Calm flat water, no rapids: the wake darkens by wake_dark exactly.
    const render::FlowSample dry = flow_of({0.3F, 0.0F}, 0.0F, 0.0F);
    const render::FlowSample lee = flow_of({0.3F, 0.0F}, 0.0F, 1.0F);
    const render::FlowSample half = flow_of({0.3F, 0.0F}, 0.0F, 0.5F);
    for (const float py : {900.0F, 960.0F, 1031.0F}) {
        const float l0 = shade_at(p, dry, 5.0F, 0.0F, 300.0F, py).light;
        const float l1 = shade_at(p, lee, 5.0F, 0.0F, 300.0F, py).light;
        const float lh = shade_at(p, half, 5.0F, 0.0F, 300.0F, py).light;
        CHECK(l1 == doctest::Approx(l0 - p.wake_dark).epsilon(1e-5).scale(1.0));
        CHECK(lh == doctest::Approx(l0 - 0.5F * p.wake_dark).epsilon(1e-5).scale(1.0));
    }
    // The streak layer alone (the whitewater layer) is raised by 1 + wake_boost at full
    // wake: (light + wake_dark - base) is 1.6 times the streak part without the wake.
    render::RiverParams streaks = p;
    streaks.count = 1;
    streaks.layers[0] = p.layers[3]; // the rapids layer of the default table
    REQUIRE(streaks.layers[0].rapids == 1.0F);
    double sum0 = 0.0, sum1 = 0.0;
    for (int i = 0; i < 400; ++i) {
        const float px = 300.0F + static_cast<float>(i * 7);
        const float l0 = shade_at(streaks, dry, 5.0F, 16.0F, px, 900.0F).light;
        const float l1 = shade_at(streaks, lee, 5.0F, 16.0F, px, 900.0F).light;
        const double part0 = static_cast<double>(l0) - static_cast<double>(streaks.rapids_base);
        const double part1 = static_cast<double>(l1) + static_cast<double>(streaks.wake_dark) -
                             static_cast<double>(streaks.rapids_base);
        sum0 += part0 * part0;
        sum1 += part1 * part1;
        CHECK(part1 == doctest::Approx((1.0 + static_cast<double>(streaks.wake_boost)) * part0)
                           .epsilon(1e-3)
                           .scale(0.01));
    }
    CHECK(sum1 > 2.0 * sum0); // 1.6 squared
    // Turbulence doubles the amplitude of a layer that fades with the zoom group's
    // detail (the glints), at full turbulence, and leaves the slow layers alone.
    render::RiverParams glints = p;
    glints.count = 1;
    glints.layers[0] = p.layers[2];
    REQUIRE(glints.layers[0].fades == 1.0F);
    render::RiverParams slow = p;
    slow.count = 1;
    slow.layers[0] = p.layers[1];
    REQUIRE(slow.layers[0].fades == 0.0F);
    for (int i = 0; i < 50; ++i) {
        const float px = 300.0F + static_cast<float>(i * 11);
        const float calm = shade_at(glints, flow_of({0.5F, 0.2F}, 0.0F), 5.0F, 0.0F, px).light;
        const float rough = shade_at(glints, flow_of({0.5F, 0.2F}, 1.0F), 5.0F, 0.0F, px).light;
        CHECK(rough == doctest::Approx(2.0F * calm).epsilon(1e-4).scale(1e-3));
        CHECK(shade_at(slow, flow_of({0.5F, 0.2F}, 1.0F), 5.0F, 0.0F, px).light ==
              shade_at(slow, flow_of({0.5F, 0.2F}, 0.0F), 5.0F, 0.0F, px).light);
    }
}

TEST_CASE(
    "river shading: standing waves (off by default) have the wavelength of E13, stand still and "
    "need a supercritical flow") {
    river::RiverConfig cfg;
    CHECK_FALSE(cfg.standing_waves);
    const render::RiverParams off = cfg.params();
    CHECK(off.standing == 0.0F);
    cfg.standing_waves = true;
    const render::RiverParams on = cfg.params();
    CHECK(on.standing == 1.0F);
    // Shallow fast water (2 units, 1.2 tiles/s: Fr 1.13); the difference of the light
    // with and without the term is the term alone.
    const float depth = 2.0F;
    const float speed = 1.2F;
    REQUIRE(froude_of(on, speed, depth) > 1.0);
    const double lambda =
        std::clamp(2.0 * kPi * speed * speed / static_cast<double>(on.gravity),
                   static_cast<double>(on.standing_min), static_cast<double>(on.standing_max));
    CHECK(lambda == doctest::Approx(0.6428).epsilon(1e-3));
    auto term = [&](std::array<float, 2> u, float px, float py, std::uint64_t t,
                    float shear = 0.0F) {
        const render::FlowSample f = flow_of(u, 0.0F, 0.0F, shear);
        return static_cast<double>(shade_at(on, f, depth, 0.0F, px, py, t).light) -
               static_cast<double>(shade_at(off, f, depth, 0.0F, px, py, t).light);
    };
    // Along the flow (tile x axis; one pixel is 1/84 of a tile along it at constant
    // py), the term is a sine of period lambda: 54 pixels.
    std::vector<double> v;
    for (int i = 0; i < 400; ++i) {
        v.push_back(term({speed, 0.0F}, 300.0F + static_cast<float>(i), 900.0F, 0));
    }
    double amp = 0.0;
    for (const double x : v) {
        amp = std::max(amp, std::fabs(x));
    }
    CHECK(amp == doctest::Approx(static_cast<double>(on.standing_amp)).epsilon(0.02));
    std::vector<double> crossings;
    for (std::size_t i = 1; i < v.size(); ++i) {
        if (v[i - 1] < 0.0 && v[i] >= 0.0) {
            crossings.push_back(static_cast<double>(i) - v[i] / (v[i] - v[i - 1]));
        }
    }
    REQUIRE(crossings.size() >= 5);
    const double period_px =
        (crossings.back() - crossings.front()) / static_cast<double>(crossings.size() - 1);
    CHECK(period_px == doctest::Approx(lambda * 84.0).epsilon(0.02));
    // It stands still: the same at another time.
    for (const int i : {0, 17, 90, 211}) {
        // (a difference of two lights: the rounding of the other layers is 1e-7)
        CHECK(std::fabs(term({speed, 0.0F}, 300.0F + static_cast<float>(i), 900.0F, 0) -
                        term({speed, 0.0F}, 300.0F + static_cast<float>(i), 900.0F, 777)) < 1e-5);
    }
    // Crests are perpendicular to the flow: with the flow along the tile y axis, moving
    // 2 tiles along the tile x axis (84 px and 42 px along the screen: constant tile y)
    // changes nothing.
    for (const int i : {0, 23, 61}) {
        CHECK(term({0.0F, speed}, 300.0F + static_cast<float>(i), 900.0F, 0) ==
              doctest::Approx(
                  term({0.0F, speed}, 300.0F + static_cast<float>(i) + 84.0F, 900.0F + 42.0F, 0))
                  .epsilon(1e-3)
                  .scale(1e-3));
    }
    // A slower flow has a shorter wave (u^2): half the speed, a quarter of the wavelength,
    // up to the lower limit.
    CHECK(std::clamp(2.0 * kPi * 0.6 * 0.6 / static_cast<double>(on.gravity),
                     static_cast<double>(on.standing_min), static_cast<double>(on.standing_max)) ==
          doctest::Approx(static_cast<double>(on.standing_min)));
    // Nothing below the lower Froude limit (0.5): a slow flow over 5 units of water.
    for (const int i : {0, 5, 40}) {
        CHECK(term({0.4F, 0.0F}, 300.0F + static_cast<float>(i), 900.0F, 0) == 0.0);
    }
    // The amplitude fades with the shear (the phase is aliased where the direction
    // changes): a strong shear removes it.
    for (const int i : {0, 13, 29}) {
        CHECK(term({speed, 0.0F}, 300.0F + static_cast<float>(i), 900.0F, 0, 20.0F) == 0.0);
    }
    // Bounded: never above standing_amp.
    CHECK(amp <= static_cast<double>(on.standing_amp) * 1.0001);
}

TEST_CASE(
    "objects in a river: the waterline ripple drifts with the flow, and stays put without one") {
    // iso::object_river_ripple: two oblique plane waves carried by the two-phase flow
    // map. For a small step in time and the matching shift along the flow the ripple
    // is nearly the same; without the shift it is not. With no flow it does not
    // move at all; and it wraps seamlessly at the hour.
    const float s = 1.0F;
    double err_adv = 0.0, err_still = 0.0, moved = 0.0;
    int n = 0;
    for (const float vpx : {120.0F, -90.0F}) {
        for (const float vpy : {0.0F, 60.0F}) {
            for (int i = 0; i < 80; ++i) {
                const float fx = 300.0F + static_cast<float>(i) * 3.3F,
                            fy = 900.0F + static_cast<float>(i) * 1.7F;
                const std::uint64_t t = 400 + static_cast<std::uint64_t>(i) * 13;
                const std::uint64_t dt = 20;
                const float r0 = render::object_river_ripple(fx, fy, s, t, vpx, vpy);
                const float shift = static_cast<float>(dt) * 0.001F;
                const float adv = render::object_river_ripple(fx + vpx * shift, fy + vpy * shift, s,
                                                              t + dt, vpx, vpy);
                const float still = render::object_river_ripple(fx, fy, s, t + dt, vpx, vpy);
                err_adv += std::fabs(static_cast<double>(adv - r0));
                err_still += std::fabs(static_cast<double>(still - r0));
                moved += std::fabs(static_cast<double>(r0));
                ++n;
            }
        }
    }
    MESSAGE("mean |change| over 20 ms: carried by the flow "
            << err_adv / n << ", staying put " << err_still / n << ", mean |ripple| " << moved / n);
    CHECK(err_adv < 0.25 * err_still);
    CHECK(moved / n > 0.1); // a ripple of about a pixel
    // No flow: the pattern is cyclic in the period (the two phases morph it, nothing
    // drifts), and the same at the same time of two cycles.
    for (const int i : {0, 7, 41}) {
        const float fx = 300.0F + static_cast<float>(i) * 3.3F;
        CHECK(render::object_river_ripple(fx, 900.0F, s, 100, 0.0F, 0.0F) ==
              doctest::Approx(render::object_river_ripple(fx, 900.0F, s, 100 + 1600, 0.0F, 0.0F))
                  .epsilon(1e-4)
                  .scale(1.0));
    }
    // Its size is the legacy ripple's: the standard deviation of 0.4 sin + 0.25 sin is
    // 0.334 pixels at group 0 (s = 1) and scales with s.
    for (const float sc : {1.0F, 0.5F}) {
        double sum = 0.0, sum2 = 0.0;
        int m = 0;
        for (int j = 0; j < 60; ++j) {
            for (int i = 0; i < 60; ++i) {
                const float r = render::object_river_ripple(
                    300.0F + static_cast<float>(i) * 5.3F, 900.0F + static_cast<float>(j) * 4.1F,
                    sc, static_cast<std::uint64_t>(100 + j * 7), 30.0F, 10.0F);
                sum += static_cast<double>(r);
                sum2 += static_cast<double>(r) * static_cast<double>(r);
                ++m;
            }
        }
        const double mean = sum / m;
        const double sd = std::sqrt(sum2 / m - mean * mean);
        MESSAGE("ripple standard deviation at s = " << sc << ": " << sd << " (legacy "
                                                    << 0.334 * static_cast<double>(sc) << ")");
        CHECK(sd == doctest::Approx(0.334 * static_cast<double>(sc)).epsilon(0.2));
        CHECK(std::fabs(mean) < 0.1 * sd * 3.0);
    }
    // The hour: the period divides it, the time wraps.
    CHECK(3600000.0F / render::kObjectRipplePeriodMs ==
          std::floor(3600000.0F / render::kObjectRipplePeriodMs));
    for (const int i : {0, 7, 41}) {
        const float fx = 300.0F + static_cast<float>(i) * 3.3F;
        CHECK(render::object_river_ripple(fx, 900.0F, s, 700, 80.0F, 20.0F) ==
              render::object_river_ripple(fx, 900.0F, s, 700 + 3600000, 80.0F, 20.0F));
    }
    // At the zoom group's scale s the pattern is s times as long and as high: the ripple
    // of (s fx, s fy) at scale s is s times the ripple of (fx, fy) at scale 1.
    for (const int i : {0, 7, 41}) {
        const float fx = 300.0F + static_cast<float>(i) * 3.3F, fy = 900.0F + static_cast<float>(i);
        CHECK(render::object_river_ripple(2.0F * fx, 2.0F * fy, 2.0F, 0, 0.0F, 0.0F) ==
              doctest::Approx(2.0F * render::object_river_ripple(fx, fy, 1.0F, 0, 0.0F, 0.0F))
                  .epsilon(1e-3)
                  .scale(1.0));
    }
}

TEST_CASE("river mouths: a river pixel at the sea has the look of the sea, the river look fades in "
          "with the "
          "shore distance, aeration included") {
    // sea::river_sea_mix (used by shade_sea_flow, Scene::blend_rivers_into_sea; Q8):
    // at kb = 0 the shade is the sea's whatever the river's, bit for bit; at kb = 1 the
    // river's; in between linear in every component, aeration too (the sea has none).
    render::Shade river, sea;
    river.light = 0.07F, river.foam = 0.0F, river.tint = 0.15F, river.aeration = 0.35F;
    sea.light = -0.02F, sea.foam = 0.3F, sea.tint = 0.05F, sea.aeration = 0.0F;
    const render::Shade zero = render::sea::river_sea_mix(river, sea, 0.0F);
    CHECK(zero.light == sea.light);
    CHECK(zero.foam == sea.foam);
    CHECK(zero.tint == sea.tint);
    CHECK(zero.aeration == sea.aeration);
    const render::Shade one = render::sea::river_sea_mix(river, sea, 1.0F);
    CHECK(one.light == doctest::Approx(river.light));
    CHECK(one.foam == doctest::Approx(river.foam));
    CHECK(one.tint == doctest::Approx(river.tint));
    CHECK(one.aeration == doctest::Approx(river.aeration));
    for (const float kb : {0.25F, 0.5F, 0.8F}) {
        const render::Shade m = render::sea::river_sea_mix(river, sea, kb);
        CHECK(m.light == doctest::Approx(sea.light + (river.light - sea.light) * kb));
        CHECK(m.foam == doctest::Approx(sea.foam + (river.foam - sea.foam) * kb));
        CHECK(m.tint == doctest::Approx(sea.tint + (river.tint - sea.tint) * kb));
        CHECK(m.aeration == doctest::Approx(river.aeration * kb));
    }
    // The pixel: the sea's shade at the mouth is the pixel the sea's shade gives.
    const render::Px water{40, 110, 140, 255};
    const render::Px at_mouth =
        render::shade_apply(water, render::sea::river_sea_mix(river, sea, 0.0F));
    CHECK(at_mouth == render::shade_apply(water, sea));
}
