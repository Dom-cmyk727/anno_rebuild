// Tests of the coastal wave simulation (anno::render::fluid; no game files
// needed): the tests of crates/render/src/fluid.rs, and parity with the Rust
// reference via crates/render/examples/digest_fluid.rs.
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/render/fluid.hpp>
#include <anno/render/numeric.hpp>

namespace fluid = anno::render::fluid;
using fluid::CellRect;
using fluid::FluidConfig;
using fluid::FluidGrid;
using fluid::FluidWindow;

namespace {

std::uint64_t fnv_step(std::uint64_t h, float f) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &f, sizeof bits);
    for (int k = 0; k < 4; ++k) {
        h ^= (bits >> (8 * k)) & 0xFFU;
        h *= 0x0100'0000'01B3ULL;
    }
    return h;
}

constexpr std::uint64_t kFnvBasis = 0xCBF2'9CE4'8422'2325ULL;

std::uint64_t fnv_f32(const std::vector<float>& v) {
    std::uint64_t h = kFnvBasis;
    for (const float f : v) {
        h = fnv_step(h, f);
    }
    return h;
}

/// Depth, then (η, u, v, foam) of every storage cell.
std::uint64_t grid_digest(const FluidGrid& g) {
    std::uint64_t h = kFnvBasis;
    for (const float f : g.depth) {
        h = fnv_step(h, f);
    }
    for (const fluid::Cell& c : g.state) {
        for (const float f : c) {
            h = fnv_step(h, f);
        }
    }
    return h;
}

/// The synthetic coast of digest_fluid.rs: deep water in the west, a beach
/// rising to land in the east, a small island around tile (5, 4).
float coast(float x, float y) {
    const float beach =
        16.0F - 2.0F * anno::render::fmaxf(x - 2.0F, 0.0F) + 0.5F * std::abs(y - 8.0F);
    const float dx = x - 5.0F, dy = y - 4.0F;
    const float island = (dx * dx + dy * dy) * 2.0F - 3.0F;
    return anno::render::fminf(beach, island);
}

FluidConfig small(std::vector<std::uint32_t> cells_per_tile) {
    FluidConfig cfg;
    cfg.size = 64;
    cfg.cells_per_tile = std::move(cells_per_tile);
    return cfg;
}

} // namespace

TEST_CASE("fluid window recentres in strips") {
    const FluidConfig cfg = small({8, 4, 2});
    FluidWindow w = FluidWindow::centred(cfg, 0, {10.0F, 10.0F});
    CHECK(w.origin == std::pair<std::int64_t, std::int64_t>{80 - 32, 80 - 32});
    CHECK(w.recentre({10.5F, 10.0F}, 8).empty());
    // 2 tiles = 16 cells right, 1 tile = 8 cells down.
    const std::vector<CellRect> next = w.recentre({12.0F, 11.0F}, 8);
    CHECK(w.origin == std::pair<std::int64_t, std::int64_t>{64, 56});
    CHECK(next == std::vector<CellRect>{{112, 56, 128, 120}, {64, 112, 112, 120}});
    std::int64_t cells = 0;
    for (const CellRect& r : next) {
        cells += (r.x1 - r.x0) * (r.y1 - r.y0);
    }
    CHECK(cells == 64 * 64 - 48 * 56);
    // Separate statement: the operands of == are evaluated in an unspecified
    // order, and w.rect() must see the window after the jump.
    const std::vector<CellRect> jump = w.recentre({100.0F, 100.0F}, 8);
    CHECK(jump == std::vector<CellRect>{w.rect()});
}

TEST_CASE("fluid swell is seeded and bounded") {
    const FluidConfig cfg;
    const fluid::Swell a = fluid::swell_components(cfg);
    CHECK(a == fluid::swell_components(cfg));
    float total = 0.0F;
    for (const auto& c : a) {
        total += c.amplitude;
    }
    CHECK(total < 8.0F * cfg.swell_amplitude);
    for (const auto& c : a) {
        const float k = std::sqrt(c.k[0] * c.k[0] + c.k[1] * c.k[1]);
        CHECK(std::abs(c.omega / k - cfg.deep_speed) < 1e-4F);
    }
}

// A wave reaching a beach: the solver stays bounded (stable), breaks in
// shallow water (foam), runs up onto land (cells above the waterline get
// wet) and drains back (they also fall dry again).
TEST_CASE("fluid beach is stable, breaks and runs up") {
    const FluidConfig cfg = small({4, 4, 4});
    const FluidWindow window = FluidWindow::centred(cfg, 0, {8.0F, 8.0F});
    // Deep in the west, a slope rising to land in the east.
    auto depth = [](float x, float) { return 16.0F - 2.0F * anno::render::fmaxf(x - 2.0F, 0.0F); };
    FluidGrid g(cfg, window, depth, 0.0F);
    std::vector<std::size_t> land;
    for (std::size_t i = 0; i < g.depth.size(); ++i) {
        if (g.depth[i] >= -1.5F && g.depth[i] < -0.1F) {
            land.push_back(i);
        }
    }
    std::vector<bool> ever_wet(g.depth.size(), false);
    std::size_t wet_then_dry = 0;
    float t = 0.0F;
    for (int s = 0; s < 900; ++s) {
        t += cfg.dt;
        g.step(t);
        for (const std::size_t i : land) {
            const bool wet = g.state[i][0] + g.depth[i] > fluid::kWet;
            if (wet) {
                ever_wet[i] = true;
            } else if (ever_wet[i]) {
                ++wet_then_dry;
            }
        }
    }
    float max_eta = 0.0F, foam = 0.0F;
    for (const fluid::Cell& s : g.state) {
        max_eta = anno::render::fmaxf(max_eta, std::abs(s[0]));
        foam += s[3];
    }
    CHECK(std::isfinite(max_eta));
    CHECK(max_eta < 20.0F);
    CHECK(foam > 0.0F);
    bool run_up = false;
    for (const std::size_t i : land) {
        run_up = run_up || ever_wet[i];
    }
    CHECK_MESSAGE(run_up, "no run-up");
    CHECK_MESSAGE(wet_then_dry > 0, "no backwash");
}

// The run of crates/render/examples/digest_fluid.rs. Rust's f32 sin/cos/exp
// and C++'s std::sin/cos/exp call the platform's libm, so the port must
// match the Rust example bit for bit on the same C library. Expected
// digests per libm (the forcing digest identifies it): Apple macOS (Apple
// Silicon, 2026-10-18) and glibc 2.39 (aarch64 and x86_64 alike, Rust 1.75
// and g++ 13, fluid review 2026-10-18), MSVC's UCRT (Windows 11 26H2, MSVC
// 19.44 and Rust 1.99.1 x86_64-pc-windows-msvc, 2026-10-18). Other C
// libraries may round those functions differently in the last bit; there
// the state is checked against the Rust values with a tolerance only.
TEST_CASE("fluid matches the Rust reference") {
    const FluidConfig cfg = small({4, 4, 4});
    const fluid::Swell comps = fluid::swell_components(cfg);
    std::vector<float> swell;
    for (const auto& c : comps) {
        swell.insert(swell.end(), {c.k[0], c.k[1], c.omega, c.phase, c.amplitude});
    }
    std::vector<float> samples;
    for (int j = 0; j < 16; ++j) {
        for (int i = 0; i < 16; ++i) {
            const float fi = static_cast<float>(i), fj = static_cast<float>(j);
            const fluid::Forcing f = fluid::forcing(
                cfg, comps, {fi * 0.75F - 3.0F, fj * 1.25F - 5.0F}, (fi + fj) * 0.37F);
            samples.insert(samples.end(), {f.eta, f.u, f.v});
        }
    }
    FluidGrid g(cfg, FluidWindow::centred(cfg, 0, {8.0F, 8.0F}), coast, 0.0F);
    const std::uint64_t init = grid_digest(g);
    float t = 0.0F;
    for (int s = 0; s < 300; ++s) {
        t += cfg.dt;
        g.step(t);
    }
    const std::uint64_t step300 = grid_digest(g);
    // Scroll the window (ring buffer): 1.5 tiles = 6 cells right, 2 down.
    const std::vector<CellRect> next = g.window.recentre({9.5F, 8.5F}, 4);
    for (const CellRect& r : next) {
        g.fill(r, coast, t);
    }
    CHECK(g.window.origin == std::pair<std::int64_t, std::int64_t>{6, 2});
    CHECK(next.size() == 2);
    const std::uint64_t filled = grid_digest(g);
    for (int s = 0; s < 300; ++s) {
        t += cfg.dt;
        g.step(t);
    }
    const std::uint64_t step600 = grid_digest(g);

    // Swell components (k, omega, phase, amplitude) and the state at a few
    // world cells (x, y, η, u, v, foam) as printed by digest_fluid.rs. The
    // tolerances hold with sin/cos/exp perturbed by one ulp (checked on the
    // reference platform).
    const std::array<std::array<float, 5>, fluid::kSwellComponents> ref_swell = {{
        {0.76102614F, 0.106145225F, 2.3051786F, 1.7487621F, 1.1785754F},
        {0.2319647F, 0.67711765F, 2.1472454F, 4.336546F, 0.740658F},
        {-0.25328773F, 0.708802F, 2.2580955F, 6.016823F, 0.6594362F},
        {-0.5791053F, 0.48343906F, 2.2631145F, 2.1992712F, 0.39889702F},
        {-0.47301912F, -0.10510594F, 1.4536675F, 2.3840692F, 0.2660796F},
        {-0.25538552F, -0.5237386F, 1.7480603F, 4.8071103F, 0.3742523F},
        {0.08472605F, -0.50920504F, 1.548617F, 3.0226986F, 0.6343092F},
        {0.7072486F, -0.29422343F, 2.2980235F, 3.7088478F, 0.82091F},
    }};
    for (std::size_t c = 0; c < fluid::kSwellComponents; ++c) {
        for (std::size_t k = 0; k < 5; ++k) {
            CAPTURE(c);
            CAPTURE(k);
            CHECK(std::abs(swell[c * 5 + k] - ref_swell[c][k]) < 1e-5F);
        }
    }
    struct Ref {
        std::int64_t x, y;
        fluid::Cell s;
    };
    const std::array<Ref, 5> refs = {{
        {10, 20, {0.83252203F, 0.1866137F, -0.28125408F, 4.1093637e-5F}},
        {30, 20, {-1.9275645F, -0.19287032F, -0.08583204F, 0.0016611203F}},
        {38, 30, {-0.69443715F, -0.068657674F, -0.06974187F, 0.005440602F}},
        {42, 40, {0.1875F, 0.0F, 0.0F, 0.026053516F}},
        {20, 16, {2.9375F, 0.0F, 0.0F, 0.06769042F}},
    }};
    const auto n = static_cast<std::size_t>(g.window.size);
    for (const Ref& r : refs) {
        const auto [sx, sy] = g.window.storage(r.x, r.y);
        const fluid::Cell& s = g.state[sy * n + sx];
        for (std::size_t k = 0; k < 4; ++k) {
            CAPTURE(r.x);
            CAPTURE(r.y);
            CAPTURE(k);
            CHECK(std::abs(s[k] - r.s[k]) < 1e-4F);
        }
    }
    // swell, forcing, init, step300, filled, step600 of digest_fluid.rs.
    const std::array<std::uint64_t, 6> digests = {fnv_f32(swell), fnv_f32(samples), init,
                                                  step300,        filled,           step600};
    const std::array<std::array<std::uint64_t, 6>, 3> known = {{
        // Apple macOS libm.
        {0x7a4c'207a'7612'c9a9ULL, 0x182b'7b0a'a2d6'41d1ULL, 0x0522'553a'cc2d'39b4ULL,
         0x1543'fb50'aa43'8ec3ULL, 0x7561'931c'd434'1886ULL, 0x811b'61d2'476f'2784ULL},
        // glibc 2.39.
        {0x7a4c'207a'7612'c9a9ULL, 0x4c75'2188'16a7'48e2ULL, 0x2c14'2892'd0bb'57c0ULL,
         0xb2e1'ba66'd842'60c6ULL, 0x8a51'7b3c'a127'9ecdULL, 0x5c79'6f1d'58ea'4fabULL},
        // MSVC's UCRT.
        {0x7a4c'207a'7612'c9a9ULL, 0xfefe'eb84'81cd'9f7bULL, 0x8859'adee'c077'd750ULL,
         0xea77'ca18'908e'904aULL, 0xc49e'31f3'2dc3'0afaULL, 0xf34f'05b6'901d'6dddULL},
    }};
    std::string printed;
    for (const std::uint64_t d : digests) {
        char buf[18];
        std::snprintf(buf, sizeof buf, " %016llx", static_cast<unsigned long long>(d));
        printed += buf;
    }
    MESSAGE("digests" << printed);
    CHECK(digests[0] == known[0][0]);
#if defined(__APPLE__) || defined(__GLIBC__) || defined(_MSC_VER)
    // A known libm: bit-identical with the Rust reference on it.
    const auto* table = digests[1] == known[0][1]   ? &known[0]
                        : digests[1] == known[1][1] ? &known[1]
                        : digests[1] == known[2][1] ? &known[2]
                                                    : nullptr;
    REQUIRE_MESSAGE(table != nullptr, "forcing digest of an unknown libm: add its Rust digests");
    CHECK(digests == *table);
#else
    // Other libms: the forcing samples stay bounded by the summed
    // amplitudes; the digests are not compared.
    float amplitude = 0.0F;
    for (const auto& c : comps) {
        amplitude += c.amplitude;
    }
    for (std::size_t i = 0; i < samples.size(); i += 3) {
        CHECK(std::abs(samples[i]) <= amplitude);
    }
    static_cast<void>(digests);
#endif
}
