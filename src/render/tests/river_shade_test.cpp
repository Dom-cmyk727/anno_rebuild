// Tests of the river flow shading and its lattice (docs/render/rivers.md 2.8,
// 3.3, 8, 10.1; stage S5). Synthetic data only: no game files.
//
// The shading is a function of (baked flow, pixel, time); every rule of the
// design gets a case that fails if the rule is implemented wrongly: features
// move with the flow (a shift estimate against the prediction, not just "it
// changes"), Galilean invariance of the flow-map coordinates, no pulsing,
// seamless hourly wrap, calibrated light variance, bounded light, the shear
// fade, the river's own water plane, and the lattice (valid-weighted
// interpolation, quantisation, the extension past the wet cells).
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

#include <doctest/doctest.h>

#include <anno/render/iso.hpp>
#include <anno/render/river.hpp>
#include <anno/render/sea.hpp>

namespace river = anno::render::river;
namespace render = anno::render;

namespace {

struct Stat {
    double mean = 0, sd = 0;
};

Stat stat(const std::vector<double>& v) {
    double m = 0;
    for (const double x : v) {
        m += x;
    }
    m /= static_cast<double>(v.size());
    double q = 0;
    for (const double x : v) {
        q += (x - m) * (x - m);
    }
    return {m, std::sqrt(q / static_cast<double>(v.size()))};
}

render::RiverView view0() {
    return {84.0F, 42.0F, 2.059F};
}

render::FlowSample uniform(std::array<float, 2> u, float level = 32.0F, float shear = 0.0F) {
    render::FlowSample f;
    f.vx = u[0];
    f.vy = u[1];
    f.level = level;
    f.valid = 1.0F;
    f.shear = shear;
    return f;
}

float light_at(const render::RiverParams& p, const render::FlowSample& f, float px, float py,
               std::uint64_t t, float slope = 0.0F, float detail = 1.0F) {
    return render::river_flow_shade(px, py, f, 5.0F, slope, t, detail, p, view0()).light;
}

/// Light of the flow shading over a patch of pixels (step `step`, `n` x `n`).
std::vector<double> patch_light(const render::RiverParams& p, const render::FlowSample& f,
                                std::uint64_t t, int n = 100, int step = 8, float detail = 1.0F) {
    std::vector<double> out;
    for (int j = 0; j < n; ++j) {
        for (int i = 0; i < n; ++i) {
            out.push_back(static_cast<double>(light_at(p, f, static_cast<float>(i * step) + 300.0F,
                                                       static_cast<float>(j * step) + 900.0F, t,
                                                       0.0F, detail)));
        }
    }
    return out;
}

std::vector<double> legacy_patch(std::uint64_t t, int n = 100, int step = 8) {
    std::vector<double> out;
    for (int j = 0; j < n; ++j) {
        for (int i = 0; i < n; ++i) {
            const float px = static_cast<float>(i * step) + 300.0F;
            // The sea plane: 32 HEIGHT units of lift (65 px at group 0).
            const float py = static_cast<float>(j * step) + 900.0F + 65.0F;
            const std::pair<float, float> pos{px / 84.0F + py / 42.0F, py / 42.0F - px / 84.0F};
            out.push_back(static_cast<double>(render::river_shade(pos, 5.0F, 0.0F, t).light));
        }
    }
    return out;
}

/// One ripple layer alone (the first of the default table), for exact laws.
render::RiverParams one_layer(std::size_t which = 1) {
    river::RiverConfig cfg;
    const river::LayerConfig l = cfg.layers[which];
    cfg.layers = {l};
    return cfg.params();
}

} // namespace

TEST_CASE("river shading config: the default tables are consistent and every check bites") {
    const river::RiverConfig cfg;
    CHECK(cfg.validate(512, 3).empty());
    CHECK(cfg.layers.size() <= render::kMaxRiverLayers);
    CHECK(cfg.params().count == cfg.layers.size());
    // Hourly wrap: every period divides 3,600,000 ms (checked here by hand).
    for (const auto& l : cfg.layers) {
        const auto period = static_cast<std::uint64_t>(l.period_ms);
        CHECK(static_cast<float>(period) == l.period_ms);
        CHECK(3600000ULL % period == 0);
    }
    for (const std::uint32_t s : cfg.lattice_stride) {
        CHECK(512U % s == 0);
    }
    CHECK(cfg.lattice_stride.size() == render::kGroups);
    // Each rule of validate() fails when broken.
    {
        river::RiverConfig c;
        c.layers[0].period_ms = 1700.0F;
        CHECK_FALSE(c.validate(512, 3).empty());
    }
    {
        river::RiverConfig c;
        c.layers[0].period_ms = 2400.5F;
        CHECK_FALSE(c.validate(512, 3).empty());
    }
    {
        river::RiverConfig c;
        c.layers.assign(render::kMaxRiverLayers + 1, c.layers[0]);
        CHECK_FALSE(c.validate(512, 3).empty());
        c.layers.pop_back();
        CHECK(c.validate(512, 3).empty());
        CHECK(c.params().count == render::kMaxRiverLayers);
    }
    {
        river::RiverConfig c;
        c.lattice_stride = {8, 7, 4};
        CHECK_FALSE(c.validate(512, 3).empty());
        c.lattice_stride = {8, 4};
        CHECK_FALSE(c.validate(512, 3).empty());
        c.lattice_stride = {8, 4, 0};
        CHECK_FALSE(c.validate(512, 3).empty());
        // The GPU carries the stride in 8 bits: 256 and 512 divide the chunk but are refused
        // (else the GPU would drop the lattice while the CPU draws it).
        c.lattice_stride = {8, 4, 256};
        CHECK_FALSE(c.validate(512, 3).empty());
        c.lattice_stride = {512, 4, 4};
        CHECK_FALSE(c.validate(512, 3).empty());
        c.lattice_stride = {river::kMaxLatticeStride + 1, 4, 4};
        CHECK_FALSE(c.validate(1024, 3).empty());
        c.lattice_stride = {128, 64, 32};
        CHECK(c.validate(512, 3).empty());
    }
    {
        river::RiverConfig c;
        c.velocity_max = 9.0F;
        CHECK_FALSE(c.validate(512, 3).empty());
        c = river::RiverConfig{};
        c.layers[1].scale = 0.0F;
        CHECK_FALSE(c.validate(512, 3).empty());
        c = river::RiverConfig{};
        c.shear_to = c.shear_from;
        CHECK_FALSE(c.validate(512, 3).empty());
    }
}

TEST_CASE("river flow shading: the flow-map coordinates are Galilean") {
    // Features move with the flow: c_k(x + u dt, t + dt) = c_k(x, t) while a
    // phase does not wrap (rivers.md E17, d c_k / dt = -u s).
    const river::RiverConfig cfg;
    std::uint32_t seed = 12345;
    auto rnd = [&seed] {
        seed = seed * 1664525U + 1013904223U;
        return static_cast<float>(seed >> 8) / 16777216.0F;
    };
    int checked = 0;
    for (const auto& lc : cfg.layers) {
        const render::RiverLayer l =
            cfg.params().layers[static_cast<std::size_t>(&lc - cfg.layers.data())];
        for (int k = 0; k < 40; ++k) {
            const std::array<float, 2> x{rnd() * 20.0F, rnd() * 20.0F};
            const std::array<float, 2> u{rnd() * 2.0F - 1.0F, rnd() * 2.0F - 1.0F};
            const float psi = 0.3F;
            // A time at which neither phase is near its wrap, and a small dt.
            const auto period = static_cast<std::uint64_t>(l.period_ms);
            const std::uint64_t t = static_cast<std::uint64_t>(rnd() * 1000.0F) * 1000ULL +
                                    static_cast<std::uint64_t>(0.1F * l.period_ms);
            const std::uint64_t dt = std::max<std::uint64_t>(1, period / 40);
            const float phi0 = static_cast<float>(t % period) / l.period_ms + psi;
            const float ph = phi0 - std::floor(phi0);
            if (ph > 0.4F && ph < 0.55F) {
                continue; // phase 1 would wrap (ph + 0.5 near 1)
            }
            if (ph > 0.9F) {
                continue;
            }
            const auto a = render::flow_phases(x, u, t, l, psi);
            const float dts = static_cast<float>(dt) * 0.001F;
            const auto b =
                render::flow_phases({x[0] + u[0] * dts, x[1] + u[1] * dts}, u, t + dt, l, psi);
            for (std::size_t c = 0; c < 2; ++c) {
                // c_k is stretched along u but still the same point of the
                // advected pattern: equal at x and at the moved x.
                if (std::fabs(a.w[c] - b.w[c]) < 0.5F) { // not across a weight kink
                    CHECK(a.c[c][0] == doctest::Approx(b.c[c][0]).epsilon(1e-4).scale(1.0F));
                    CHECK(a.c[c][1] == doctest::Approx(b.c[c][1]).epsilon(1e-4).scale(1.0F));
                }
            }
            ++checked;
        }
    }
    CHECK(checked > 40);
    // The weights of the pair sum to one (E17) at any time and place.
    for (int k = 0; k < 200; ++k) {
        const auto a = render::flow_phases({rnd() * 9.0F, rnd() * 9.0F}, {0.4F, -0.2F},
                                           static_cast<std::uint64_t>(rnd() * 3.0e6F),
                                           cfg.params().layers[1], rnd());
        CHECK(a.w[0] + a.w[1] == doctest::Approx(1.0F).epsilon(1e-5));
        CHECK(a.w[0] >= 0.0F);
        CHECK(a.w[1] >= 0.0F);
    }
}

TEST_CASE("river flow shading: a feature moves downstream with exactly the flow") {
    // End to end through river_flow_shade: the light pattern at t + dt equals
    // the pattern at t shifted by the screen displacement of the flow. The
    // shift is estimated by minimising the squared difference over candidate
    // shifts (the shading is a function: evaluated at shifted pixels, no
    // image resampling), averaged over three moments of the cross-fade. One
    // layer, uniform flow; the case of a flow that varies in space, far from
    // the origin, is the next one, and Q5 on real rivers is in
    // river_flow_corpus_test.
    river::RiverConfig cfg;
    cfg.layers = {cfg.layers[1]};
    const render::RiverParams p = cfg.params();
    const float tw = 84.0F, th = 42.0F;
    struct Case {
        std::array<float, 2> u;
        float level;
    };
    for (const Case c : {Case{{1.0F, 0.0F}, 32.0F}, Case{{0.3F, 0.8F}, 32.0F},
                         Case{{-0.7F, 0.4F}, 32.0F}, Case{{1.0F, 0.0F}, 80.0F}}) {
        const render::FlowSample f = uniform(c.u, c.level);
        const std::uint64_t dt = 100;
        const float dts = static_cast<float>(dt) * 0.001F;
        // Screen displacement of a point carried with u (tile axes -> screen).
        const float sx = (c.u[0] - c.u[1]) * tw * 0.5F * dts;
        const float sy = (c.u[0] + c.u[1]) * th * 0.5F * dts;
        auto cost = [&](float dx, float dy) {
            double sum = 0;
            for (const std::uint64_t t : {1000ULL, 1400ULL, 1800ULL}) {
                for (int j = 0; j < 24; ++j) {
                    for (int i = 0; i < 24; ++i) {
                        const float px = 500.0F + static_cast<float>(i) * 5.0F;
                        const float py = 700.0F + static_cast<float>(j) * 5.0F;
                        const double a = light_at(p, f, px, py, t + dt);
                        const double b = light_at(p, f, px - dx, py - dy, t);
                        sum += (a - b) * (a - b);
                    }
                }
            }
            return sum;
        };
        double best = 1e30;
        float bx = 0, by = 0;
        for (float dx = -6.0F; dx <= 6.0F; dx += 0.2F) {
            for (float dy = -6.0F; dy <= 6.0F; dy += 0.2F) {
                const double v = cost(dx, dy);
                if (v < best) {
                    best = v;
                    bx = dx;
                    by = dy;
                }
            }
        }
        INFO("u (" << c.u[0] << ", " << c.u[1] << ") level " << c.level << ": predicted (" << sx
                   << ", " << sy << ") px, found (" << bx << ", " << by << ")");
        // The cross-fade of the two phases drifts the estimate a little (a
        // velocity bias of ~15 %): within 30 % of the displacement and 0.15 px.
        const float error = std::hypot(bx - sx, by - sy);
        CHECK(error <= 0.3F * std::hypot(sx, sy) + 0.15F);
        // The prediction is clearly better than standing still and than the
        // mirrored (upstream) shift.
        CHECK(cost(sx, sy) < 0.5 * cost(0.0F, 0.0F));
        CHECK(cost(sx, sy) < 0.5 * cost(-sx, -sy));
    }
}

TEST_CASE("river flow shading: the pattern lives on the river's own water plane") {
    // A point of the river surface at tile (tx, ty) and height Z appears at
    // pixel ((tx - ty) tw/2, (tx + ty) th/2 - lambda Z): its light must not
    // depend on Z (E19). On the sea plane (the legacy shimmer) a tier 48 units
    // higher is offset by 48 * 2.059 = ~99 px against the geometry.
    const render::RiverParams p = one_layer(1);
    const auto v = view0();
    int n = 0;
    for (const float tx : {3.1F, 7.7F, 12.3F}) {
        for (const float ty : {2.2F, 9.9F, 15.5F}) {
            std::array<float, 3> values{};
            std::size_t k = 0;
            for (const float z : {32.0F, 64.0F, 80.0F}) {
                const float px = (tx - ty) * v.tw * 0.5F;
                const float py = (tx + ty) * v.th * 0.5F - v.lambda * z;
                values[k++] = light_at(p, uniform({0.6F, 0.1F}, z), px, py, 777);
            }
            CHECK(values[0] == doctest::Approx(values[1]).epsilon(1e-3).scale(0.02));
            CHECK(values[0] == doctest::Approx(values[2]).epsilon(1e-3).scale(0.02));
            ++n;
        }
    }
    CHECK(n == 9);
    // ... and the light does depend on the position (the check is not vacuous).
    const float a = light_at(p, uniform({0.6F, 0.1F}), 100.0F, 100.0F, 777);
    const float b = light_at(p, uniform({0.6F, 0.1F}), 103.0F, 100.0F, 777);
    const float c = light_at(p, uniform({0.6F, 0.1F}), 100.0F, 107.0F, 777);
    CHECK((std::fabs(a - b) > 1e-5F || std::fabs(a - c) > 1e-5F));
}

namespace {

/// The spatial standard deviation of `light(px, py)` over a large patch
/// (200 x 200 samples 12 px apart: about 1,600 tiles at group 0, so that the
/// sampling noise of the estimate is ~1 %).
template <class F> double patch_sd(F&& light) {
    std::vector<double> v;
    v.reserve(200 * 200);
    for (int j = 0; j < 200; ++j) {
        for (int i = 0; i < 200; ++i) {
            v.push_back(static_cast<double>(
                light(300.0F + static_cast<float>(i * 12), 900.0F + static_cast<float>(j * 12))));
        }
    }
    return stat(v).sd;
}

/// (max - min) / mean of the patch sd over eight moments of a cycle.
template <class F> double pulsing(F&& light_at_time) {
    double lo = 1e9, hi = 0, mean = 0;
    int n = 0;
    for (std::uint64_t t = 0; t < 2400; t += 300) {
        const double sd = patch_sd([&](float px, float py) { return light_at_time(px, py, t); });
        lo = std::min(lo, sd);
        hi = std::max(hi, sd);
        mean += sd;
        ++n;
    }
    return (hi - lo) / (mean / n);
}

} // namespace

TEST_CASE("river flow shading: no pulsing, the variance-preserving blend keeps the contrast") {
    // The spatial standard deviation of the light over a patch is the same at
    // every moment of the cycle (rivers.md 10.1: < 2 %).
    const river::RiverConfig cfg;
    const auto all = cfg.params();
    render::RiverParams single = cfg.params();
    single.layers[0] = single.layers[1];
    single.count = 1;
    for (const std::array<float, 2> u : {std::array<float, 2>{0.5F, 0.2F}, {0.12F, 0.0F}}) {
        const render::FlowSample f = uniform(u);
        const double all_layers = pulsing(
            [&](float px, float py, std::uint64_t t) { return light_at(all, f, px, py, t); });
        const double one_layer_only = pulsing(
            [&](float px, float py, std::uint64_t t) { return light_at(single, f, px, py, t); });
        INFO("u (" << u[0] << ", " << u[1] << "): sd range / mean, all layers " << all_layers
                   << ", one layer " << one_layer_only);
        CHECK(all_layers < 0.02);
        CHECK(one_layer_only < 0.02);
    }
    // The control: a plain linear blend of the two phases (what the variance-
    // preserving blend is for) pulses by far more, so that this test can tell.
    {
        const render::RiverLayer layer = single.layers[0];
        const std::array<float, 2> u{0.5F, 0.2F};
        auto linear = [&](float px, float py, std::uint64_t t) {
            const float pyw = py + 2.059F * 32.0F;
            const std::array<float, 2> x{px / 84.0F + pyw / 42.0F, pyw / 42.0F - px / 84.0F};
            const std::array<float, 2> x2{x[0] - 0.15F, x[1]};
            const float psi = render::flow_psi(x);
            auto field = [&](const render::FlowPhases& ph) {
                return ph.w[0] * (render::noise3(ph.c[0][0], ph.c[0][1], 0.5F) - 0.5F) +
                       ph.w[1] * (render::noise3(ph.c[1][0], ph.c[1][1], 0.5F) - 0.5F);
            };
            return layer.amp *
                   (field(render::flow_phases(x, u, t, layer, psi)) -
                    field(render::flow_phases(x2, u, t, layer, psi))) /
                   0.15F;
        };
        const double control = pulsing(linear);
        INFO("linear blend pulses by " << control);
        CHECK(control > 0.08);
    }
}

TEST_CASE("river flow shading: the hourly time wrap is seamless") {
    // frame.time_ms is taken modulo 3,600,000: with periods that divide it the
    // phases of the last millisecond join those of the first.
    const river::RiverConfig cfg;
    const auto params = cfg.params();
    const std::array<float, 2> x{4.3F, 9.1F}, u{0.7F, -0.3F};
    for (std::size_t l = 0; l < cfg.layers.size(); ++l) {
        const render::RiverLayer& layer = params.layers[l];
        const auto at_end = render::flow_phases(x, u, 3599999, layer, 0.2F);
        const auto at_start = render::flow_phases(x, u, 0, layer, 0.2F);
        const auto wrapped = render::flow_phases(x, u, 3600000, layer, 0.2F);
        for (std::size_t k = 0; k < 2; ++k) {
            // The hour itself is the origin, bit for bit.
            CHECK(wrapped.w[k] == at_start.w[k]);
            CHECK(wrapped.c[k] == at_start.c[k]);
            // One millisecond apart: a phase step of 1 / period, so the weights
            // differ by at most 2 / period and a feature by u * 1 ms.
            CHECK(std::fabs(at_end.w[k] - at_start.w[k]) <= 2.5F / layer.period_ms);
            const float reach =
                (std::hypot(u[0], u[1]) * 0.002F + 0.5F / layer.period_ms) * layer.scale;
            CHECK(std::fabs(at_end.c[k][0] - at_start.c[k][0]) <= reach + 1e-3F);
            CHECK(std::fabs(at_end.c[k][1] - at_start.c[k][1]) <= reach + 1e-3F);
        }
    }
    // A period that does not divide the hour is why the config refuses it: its
    // phases jump at the wrap.
    render::RiverLayer bad = params.layers[1];
    bad.period_ms = 1700.0F;
    const auto e = render::flow_phases(x, u, 3599999, bad, 0.0F);
    const auto s = render::flow_phases(x, u, 0, bad, 0.0F);
    CHECK(std::fabs(e.w[0] - s.w[0]) > 0.3F);
    // The whole shading is continuous over the wrap.
    const auto p = cfg.params();
    const auto f = uniform({0.4F, 0.3F});
    for (const auto& px : {std::pair{410.0F, 950.0F}, {777.0F, 1234.0F}}) {
        CHECK(std::fabs(light_at(p, f, px.first, px.second, 3599999) -
                        light_at(p, f, px.first, px.second, 0)) < 0.004F);
        CHECK(light_at(p, f, px.first, px.second, 3600000) ==
              light_at(p, f, px.first, px.second, 0));
    }
}

TEST_CASE(
    "river flow shading: the light variance equals the legacy shimmer's for a uniform flow at "
    "full detail (calibration)") {
    // rivers.md 2.8: the amplitudes are calibrated so that the variation of the
    // light does not change when the flow shading replaces the legacy shimmer:
    // standard deviation within +-20 % FOR A UNIFORM FLOW AT FULL WAVE DETAIL,
    // at the speeds of a river (0.12 tiles/s is the floor, 0.9 the speed at the
    // mouth), averaged over flow directions (the ripples are lit from a fixed
    // side, so that a flow along the light direction shows less contrast than
    // one across it). This is the calibration of the layer table only. In a
    // real scene the glints fade with the zoom group's wave detail and the shear
    // fade removes layers, so the light there is about 0.4-0.9 of the legacy's
    // (rivers.md 15.3, OQ-083; measured on the QA islands by the corpus test
    // "the light of the flow shading against the legacy shimmer on real rivers").
    const river::RiverConfig cfg;
    const auto p = cfg.params();
    double legacy = 0;
    int n = 0;
    for (std::uint64_t t = 0; t < 2400; t += 100) {
        legacy += stat(legacy_patch(t)).sd;
        ++n;
    }
    legacy /= n;
    for (const float speed : {0.12F, 0.5F, 0.9F}) {
        double flow = 0, mean = 0;
        int m = 0;
        for (const std::array<float, 2> d : {std::array<float, 2>{1.0F, 0.0F},
                                             {0.0F, 1.0F},
                                             {0.7071F, 0.7071F},
                                             {0.7071F, -0.7071F}}) {
            for (std::uint64_t t = 0; t < 2400; t += 200) {
                const Stat s = stat(patch_light(p, uniform({speed * d[0], speed * d[1]}), t));
                flow += s.sd;
                mean += s.mean;
                ++m;
            }
        }
        flow /= m;
        mean /= m;
        INFO("speed " << speed << ": sd " << flow << " against the legacy " << legacy);
        CHECK(flow / legacy >= 0.8);
        CHECK(flow / legacy <= 1.2);
        // No brightening: the mean light stays near zero (the legacy's is ~0.001).
        CHECK(std::fabs(mean) < 0.0015);
    }
}

TEST_CASE("river flow shading: the light is bounded, finite and deterministic") {
    const river::RiverConfig cfg;
    const auto p = cfg.params();
    float worst = 0;
    std::uint32_t seed = 99;
    auto rnd = [&seed] {
        seed = seed * 1664525U + 1013904223U;
        return static_cast<float>(seed >> 8) / 16777216.0F;
    };
    float worst_calm = 0;
    for (int k = 0; k < 4000; ++k) {
        render::FlowSample f =
            uniform({(rnd() - 0.5F) * 6.0F, (rnd() - 0.5F) * 6.0F}, rnd() * 100.0F, rnd() * 3.0F);
        f.turb = k % 2 == 0 ? 0.0F : rnd();
        const float px = rnd() * 4000.0F - 500.0F, py = rnd() * 4000.0F - 500.0F;
        const auto t = static_cast<std::uint64_t>(rnd() * 7.0e6F);
        const float slope = rnd() * 30.0F;
        const render::Shade a =
            render::river_flow_shade(px, py, f, 3.0F, slope, t, rnd(), p, view0());
        const render::Shade b =
            render::river_flow_shade(px, py, f, 3.0F, slope, t, 1.0F, p, view0());
        const render::Shade c =
            render::river_flow_shade(px, py, f, 3.0F, slope, t, 1.0F, p, view0());
        REQUIRE(std::isfinite(a.light));
        REQUIRE(std::isfinite(a.tint));
        CHECK(b.light == c.light);
        CHECK(b.tint == c.tint);
        CHECK(a.foam == 0.0F); // no foam on rivers (owner 2026-10-18)
        worst = std::max({worst, std::fabs(a.light), std::fabs(b.light)});
        if (f.turb == 0.0F) {
            worst_calm = std::max({worst_calm, std::fabs(b.light)});
        }
    }
    INFO("largest |light| " << worst << ", without turbulence " << worst_calm);
    // rivers.md 10.1: |light| <= 0.2; the turbulence channel (S7) raises the
    // fine layers by up to a factor of two, so 0.3 there.
    CHECK(worst_calm <= 0.2F);
    CHECK(worst <= 0.3F);
}

TEST_CASE("river flow shading: the shear fade removes layers a cycle would smear") {
    // Thin rivers and falls: T |grad u| above 1.5 fades a layer out (rivers.md
    // 2.8); a gentle shear leaves it alone.
    const river::RiverConfig cfg;
    const auto p = cfg.params();
    auto sd_at = [&](float shear) {
        double sum = 0;
        int n = 0;
        for (std::uint64_t t = 0; t < 2400; t += 200) {
            sum += stat(patch_light(p, uniform({0.5F, 0.2F}, 32.0F, shear), t, 60, 8)).sd;
            ++n;
        }
        return sum / n;
    };
    const double calm = sd_at(0.0F), gentle = sd_at(0.1F), strong = sd_at(1.5F),
                 extreme = sd_at(4.0F);
    INFO("sd at shear 0 / 0.1 / 1.5 / 4: " << calm << " / " << gentle << " / " << strong << " / "
                                           << extreme);
    CHECK(gentle >= 0.95 * calm);
    CHECK(strong <= 0.3 * calm);
    CHECK(extreme <= 0.01 * calm);
    // Layers fade independently: at shear 0.5 (1/s) the 2400 ms layer is at
    // T * shear = 1.2 (fading) while the 800 ms glints are at 0.4 (intact).
    render::RiverParams only_glints = cfg.params();
    only_glints.layers[0] = only_glints.layers[2];
    only_glints.count = 1;
    const auto g0 =
        stat(patch_light(only_glints, uniform({0.5F, 0.2F}, 32.0F, 0.0F), 300, 60, 8)).sd;
    const auto g5 =
        stat(patch_light(only_glints, uniform({0.5F, 0.2F}, 32.0F, 0.5F), 300, 60, 8)).sd;
    CHECK(g5 >= 0.95 * g0);
}

TEST_CASE("river flow shading: the wave detail of the zoom group fades the fine layers") {
    const river::RiverConfig cfg;
    const auto p = cfg.params();
    const double full = stat(patch_light(p, uniform({0.5F, 0.2F}), 400, 100, 8, 1.0F)).sd;
    const double half = stat(patch_light(p, uniform({0.5F, 0.2F}), 400, 100, 8, 0.4F)).sd;
    const double none = stat(patch_light(p, uniform({0.5F, 0.2F}), 400, 100, 8, 0.0F)).sd;
    CHECK(full > half);
    CHECK(half > none);
    CHECK(none > 0.0); // the slow undulation and the ripples stay
    // Without detail only the layers that do not fade remain.
    render::RiverParams slow = cfg.params();
    slow.count = 2;
    const double slow_only = stat(patch_light(slow, uniform({0.5F, 0.2F}), 400, 100, 8, 1.0F)).sd;
    CHECK(none == doctest::Approx(slow_only).epsilon(1e-5));
}

TEST_CASE("river flow shading: rapids follow the surface slope and are lit by it") {
    // A slow flow (Froude 0.2 over 5 units of water), so that only the slope
    // rule makes rapids; the Froude rule has its own cases below.
    const river::RiverConfig cfg;
    const auto p = cfg.params();
    auto mean_light = [&](float slope) {
        std::vector<double> v;
        for (int j = 0; j < 60; ++j) {
            for (int i = 0; i < 60; ++i) {
                v.push_back(static_cast<double>(
                    light_at(p, uniform({0.3F, 0.0F}), static_cast<float>(i * 8) + 300.0F,
                             static_cast<float>(j * 8) + 900.0F, 500, slope)));
            }
        }
        return stat(v);
    };
    const Stat flat = mean_light(0.0F), gentle = mean_light(render::kRapidsFrom);
    const Stat steep = mean_light(16.0F);
    // Brighter and livelier where the surface falls (slope 16 units / tile is
    // a 48 unit fall over three tiles), as the legacy shimmer's rapids.
    CHECK(steep.mean > flat.mean + 0.03);
    CHECK(steep.sd > flat.sd);
    CHECK(gentle.mean < steep.mean);
}

namespace {

river::FlowPlane blank_plane(std::uint32_t nx, std::uint32_t ny, std::uint32_t stride = 4,
                             std::int32_t x0 = 512, std::int32_t y0 = 1024) {
    river::FlowPlane p;
    p.x0 = x0;
    p.y0 = y0;
    p.stride_px = stride;
    p.nx = nx;
    p.ny = ny;
    p.tile_w = 84;
    p.tile_h = 42;
    p.uv.assign(std::size_t{nx} * ny * 2, 0);
    p.aux.assign(std::size_t{nx} * ny * 4, 0);
    p.shear.assign(std::size_t{nx} * ny, 0);
    return p;
}

} // namespace

TEST_CASE("flow lattice: texels are quantised and read back at the pixel centre") {
    river::FlowPlane p = blank_plane(9, 9);
    for (std::uint32_t j = 0; j < 9; ++j) {
        for (std::uint32_t i = 0; i < 9; ++i) {
            river::set_texel(p, i, j, 0.5F, -0.25F, 1.0F, 0.0F, 0.0F, 48.25F, 8.0F);
        }
    }
    // Any pixel of the plane: the same uniform flow within the quantisation
    // (1/4096 tiles / s) and the level step (1/2 unit).
    for (const auto& px :
         {std::pair{512, 1024}, std::pair{515, 1027}, std::pair{530, 1040}, std::pair{543, 1055}}) {
        const render::FlowSample f = river::flow_at(p, px.first, px.second);
        CHECK(f.vx == doctest::Approx(0.5F).epsilon(1e-6).scale(1.0));
        CHECK(f.vy == doctest::Approx(-0.25F).epsilon(1e-6).scale(1.0));
        CHECK(f.level == doctest::Approx(48.5F));
        CHECK(f.valid == doctest::Approx(1.0F));
        CHECK(f.shear == doctest::Approx(0.0F).epsilon(1e-6));
    }
    // 1/4096 resolution: a velocity is stored as the nearest multiple.
    river::set_texel(p, 0, 0, 0.12345F, 0.0F, 1.0F, 0.0F, 0.0F, 32.0F, 8.0F);
    CHECK(static_cast<float>(p.uv[0]) / river::kVelocityScale ==
          doctest::Approx(0.12345F).epsilon(1.5e-4).scale(1.0));
    // Out of range: clamped to velocity_max (and to the int16 lattice), NaN is 0.
    river::set_texel(p, 1, 0, 50.0F, -50.0F, 1.0F, 0.0F, 0.0F, 32.0F, 8.0F);
    CHECK(p.uv[2] == 32767);
    CHECK(p.uv[3] == -32767);
    river::set_texel(p, 2, 0, std::nanf(""), 3.0F, 1.0F, 0.0F, 0.0F, 32.0F, 2.0F);
    CHECK(p.uv[4] == 0);
    CHECK(p.uv[5] == static_cast<std::int16_t>(2 * 4096));
    // Outside the plane: no flow.
    CHECK(river::flow_at(p, 511, 1024).valid == 0.0F);
    CHECK(river::flow_at(p, 512 + 33, 1024).valid == 0.0F);
    CHECK(river::flow_at(p, 512, 1024 + 33).valid == 0.0F);
    CHECK(river::flow_at(river::FlowPlane{}, 0, 0).valid == 0.0F);
}

TEST_CASE("flow lattice: a texel without flow does not drag the vector toward zero") {
    river::FlowPlane p = blank_plane(5, 5);
    // The left two columns carry a flow of 1 tile / s, the right ones none (valid 0).
    for (std::uint32_t j = 0; j < 5; ++j) {
        for (std::uint32_t i = 0; i < 5; ++i) {
            if (i < 2) {
                river::set_texel(p, i, j, 1.0F, 0.0F, 1.0F, 0.0F, 0.0F, 40.0F, 8.0F);
            } else {
                river::set_texel(p, i, j, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 8.0F);
            }
        }
    }
    // Between texel columns 1 and 2: part of the weight is valid.
    for (int dx = 4; dx <= 7; ++dx) {
        const render::FlowSample f = river::flow_at(p, 512 + dx, 1024 + 6);
        CHECK(f.vx == doctest::Approx(1.0F).epsilon(1e-6));
        CHECK(f.level == doctest::Approx(40.0F));
        CHECK(f.valid > 0.0F);
        CHECK(f.valid < 1.0F);
    }
    // The valid weight falls from 1 to 0 across the edge, monotonically.
    float last = 2.0F;
    for (int dx = 0; dx < 12; ++dx) {
        const float v = river::flow_at(p, 512 + dx, 1024 + 8).valid;
        CHECK(v <= last + 1e-6F);
        last = v;
    }
    CHECK(river::flow_at(p, 512 + 2, 1024 + 8).valid == doctest::Approx(1.0F));
    CHECK(river::flow_at(p, 512 + 12, 1024 + 8).valid == doctest::Approx(0.0F));
    // Fully invalid neighbourhood: no vector at all.
    const render::FlowSample none = river::flow_at(p, 512 + 14, 1024 + 8);
    CHECK(none.valid == 0.0F);
    CHECK(none.vx == 0.0F);
}

TEST_CASE("flow lattice: the shear is a baked channel, interpolated like the velocity") {
    // The shear |grad u| (1/s) is baked from the island's flow (Scene::flow_plane),
    // not derived from the quad of texels around a pixel: that was constant per
    // lattice cell and its fade showed as steps (finding of the S5 review).
    river::FlowPlane p = blank_plane(9, 9, 4);
    // Round trip within the step; clamped at both ends; a negative value is 0.
    river::set_texel(p, 0, 0, 0.3F, 0.1F, 1.0F, 0.0F, 0.0F, 32.0F, 8.0F, 1.30F);
    CHECK(std::fabs(static_cast<float>(p.shear[0]) * river::kShearStep - 1.30F) <=
          0.5F * river::kShearStep + 1e-6F);
    river::set_texel(p, 1, 0, 0.3F, 0.1F, 1.0F, 0.0F, 0.0F, 32.0F, 8.0F, 99.0F);
    CHECK(p.shear[1] == 255);
    river::set_texel(p, 2, 0, 0.3F, 0.1F, 1.0F, 0.0F, 0.0F, 32.0F, 8.0F, -3.0F);
    CHECK(p.shear[2] == 0);
    // A plane without the channel (a hand-made one) reads as zero shear.
    {
        river::FlowPlane q = blank_plane(5, 5);
        q.shear.clear();
        for (std::uint32_t j = 0; j < 5; ++j) {
            for (std::uint32_t i = 0; i < 5; ++i) {
                river::set_texel(q, i, j, 0.3F, 0.1F, 1.0F, 0.0F, 0.0F, 32.0F, 8.0F, 2.0F);
            }
        }
        CHECK(river::flow_at(q, 512 + 6, 1024 + 6).shear == 0.0F);
    }
    // Bilinear: texel columns carry 0 and 2 (1/s); halfway between them the
    // shear is 1, and it rises linearly with the pixel, with no step anywhere.
    river::FlowPlane q = blank_plane(9, 9, 4);
    for (std::uint32_t j = 0; j < 9; ++j) {
        for (std::uint32_t i = 0; i < 9; ++i) {
            river::set_texel(q, i, j, 0.3F, 0.1F, 1.0F, 0.0F, 0.0F, 32.0F, 8.0F,
                             i == 3 ? 2.0F : (i == 2 ? 0.0F : 1.0F));
        }
    }
    // Pixels 512 + 8 + 2 .. 512 + 12 + 2 lie between the texels at 8 and 12
    // (x = 512 + 4 i), i = 2 and 3; centres at +0.5.
    float last = -1.0F, worst_step = 0.0F;
    for (int x = 512 + 8; x < 512 + 12; ++x) {
        const float sh = river::flow_at(q, x, 1024 + 8).shear;
        const float expect = 2.0F * (static_cast<float>(x - 520) + 0.5F) / 4.0F;
        CHECK(std::fabs(sh - expect) <= river::kShearStep);
        if (last >= 0.0F) {
            worst_step = std::max(worst_step, std::fabs(sh - last));
        }
        last = sh;
    }
    CHECK(worst_step <= 2.0F / 4.0F + 0.05F); // the gradient of the channel, 0.5 per pixel
    // Valid-weighted: a texel without flow does not drag the shear to zero.
    river::FlowPlane r = blank_plane(5, 5);
    for (std::uint32_t j = 0; j < 5; ++j) {
        for (std::uint32_t i = 0; i < 5; ++i) {
            river::set_texel(r, i, j, 0.3F, 0.1F, i < 3 ? 1.0F : 0.0F, 0.0F, 0.0F, 32.0F, 8.0F,
                             i < 3 ? 1.5F : 0.0F);
        }
    }
    CHECK(river::flow_at(r, 512 + 9, 1024 + 6).shear == doctest::Approx(1.5F).epsilon(0.02));
}

TEST_CASE("flow shading: the shear fade has no steps over texel borders") {
    // A shear that changes smoothly across the fade range (0.5 .. 1.5 / T) is
    // an amplitude that changes smoothly: the light of a row of pixels never
    // jumps by more than the ripple's own change per pixel plus the gradient of
    // the amplitude. With the old per-quad shear the amplitude stepped at every
    // texel border (hard-edged flat blocks of zero light).
    const river::RiverConfig cfg;
    render::RiverParams one = cfg.params();
    one.layers[0] = one.layers[1]; // 1600 ms ripples
    one.count = 1;
    const float t_sec = one.layers[0].period_ms * 0.001F;
    // Texel columns carry the shear ramp 0 .. 3 / T across 33 texels (stride 4).
    river::FlowPlane plane = blank_plane(34, 8, 4, 0, 0);
    for (std::uint32_t j = 0; j < 8; ++j) {
        for (std::uint32_t i = 0; i < 34; ++i) {
            river::set_texel(plane, i, j, 0.4F, 0.2F, 1.0F, 0.0F, 0.0F, 32.0F, 8.0F,
                             3.0F / t_sec * static_cast<float>(i) / 33.0F);
        }
    }
    auto row_light = [&](bool constant_shear) {
        std::vector<float> v;
        for (int x = 4; x < 128; ++x) {
            render::FlowSample f = river::flow_at(plane, x, 10);
            if (constant_shear) {
                f.shear = 0.0F;
            }
            v.push_back(render::river_flow_shade(static_cast<float>(x) + 3000.0F, 2010.0F, f, 4.0F,
                                                 0.0F, 700, 1.0F, one, view0())
                            .light);
        }
        return v;
    };
    auto worst_step = [](const std::vector<float>& v) {
        float w = 0;
        for (std::size_t k = 1; k < v.size(); ++k) {
            w = std::max(w, std::fabs(v[k] - v[k - 1]));
        }
        return w;
    };
    const auto ramp = row_light(false), calm = row_light(true);
    // The row passes the whole fade (amplitude 1 -> 0): its light is smaller than
    // the calm row's overall, and its worst step is no larger than the calm
    // row's (the amplitude only ever shrinks it).
    CHECK(worst_step(ramp) <= 1.05F * worst_step(calm) + 1e-4F);
    // A 1-pixel step of the shear fade alone: smoothstep(0.5, 1.5) changes at
    // most 1.5 / 1.0 per unit of T * shear; one pixel moves T * shear by 3/128.
    // So the largest ratio of the amplitude between neighbouring pixels is small.
    float worst_ratio = 0;
    for (int x = 4; x < 127; ++x) {
        const float a = river::flow_at(plane, x, 10).shear,
                    b = river::flow_at(plane, x + 1, 10).shear;
        worst_ratio = std::max(worst_ratio, std::fabs(b - a) * t_sec);
    }
    CHECK(worst_ratio <=
          3.0F / 33.0F * 0.5F); // one pixel is a quarter texel of the 3/33 per texel ramp
}

namespace {

river::RiverMask channel_mask() {
    // A channel one tile wide along +x: columns [8, 168), rows [20, 28) at 8
    // cells per tile, a sea strip behind its end, a closed head at the start.
    river::RiverMask m(8, 0, 0, 220, 48);
    for (std::int32_t y = 20; y < 28; ++y) {
        for (std::int32_t x = 8; x < 168; ++x) {
            m.set_wet(x, y, 4.0F, 14.0F + static_cast<float>(x) * 0.01F);
        }
        for (std::int32_t x = 168; x < 176; ++x) {
            m.set_sea(x, y);
        }
    }
    return m;
}

} // namespace

TEST_CASE("river flow: the extension past the wet cells fills in the nearest flow and fades") {
    river::RiverConfig cfg;
    cfg.log_fallback = false;
    const river::RiverMask m = channel_mask();
    const river::RiverFlow f = river::RiverFlow::build(m, cfg);
    REQUIRE(f.stats().wet_cells == 160U * 8U);
    const float cf = 8.0F;
    // Inside the channel: identical to sample(), the level is the raster's.
    {
        const float tx = 12.0F, ty = 24.0F / cf + 0.5F / cf;
        const auto a = f.sample(tx, ty), b = f.sample_extended(tx, ty);
        CHECK(a.valid == doctest::Approx(1.0F));
        CHECK(b.u[0] == a.u[0]);
        CHECK(b.u[1] == a.u[1]);
        CHECK(b.valid == a.valid);
        CHECK(b.level == doctest::Approx(14.0F + (12.0F * 8.0F) * 0.01F).epsilon(0.02));
        CHECK(a.u[0] > 0.0F);
    }
    // Outside, towards the banks (rows < 20 or >= 28 are dry): the flow of the
    // nearest wet cell, valid fading from 1 at the bank to 0 at the reach.
    const float ty_bank = 20.0F / cf; // the wet edge (north bank)
    const auto edge = f.sample_extended(12.0F, ty_bank - 0.01F);
    CHECK(edge.valid > 0.9F);
    float last = 1.1F;
    for (int k = 0; k <= 24; ++k) {
        const float d = 0.1F * static_cast<float>(k); // tiles beyond the bank
        const auto s = f.sample_extended(12.0F, ty_bank - 0.05F - d);
        CHECK(s.valid <= last + 1e-6F);
        last = s.valid;
        if (d < cfg.extend_tiles - 0.3F) {
            CHECK(s.valid > 0.0F);
            // The nearest wet cell's velocity: along the channel, as at the bank.
            CHECK(s.u[0] == doctest::Approx(edge.u[0]).epsilon(0.1));
            CHECK(std::fabs(s.u[1]) < 0.2F * std::fabs(s.u[0]) + 1e-3F);
        }
        if (d > cfg.extend_tiles + 0.3F) {
            CHECK(s.valid == 0.0F);
        }
    }
    // The weight fades with the distance from the wet cells: nearly full at
    // the bank, about half a tile and a half out, little near the reach.
    CHECK(f.sample_extended(12.0F, ty_bank - 0.05F - 0.1F).valid > 0.8F);
    CHECK(f.sample_extended(12.0F, ty_bank - 0.05F - 1.0F).valid < 0.6F);
    CHECK(f.sample_extended(12.0F, ty_bank - 0.05F - 1.6F).valid < 0.2F);
    // Beyond the raster: no flow.
    CHECK(f.sample_extended(12.0F, 3.0F + 40.0F).valid == 0.0F);
    CHECK(f.sample_extended(-50.0F, 3.0F).valid == 0.0F);
    // Reach zero: the extension is off.
    cfg.extend_tiles = 0.0F;
    const river::RiverFlow g = river::RiverFlow::build(m, cfg);
    CHECK(g.sample_extended(12.0F, ty_bank - 0.3F).valid == 0.0F);
    // A pure function: two builds agree.
    const river::RiverFlow h = river::RiverFlow::build(m, river::RiverConfig{});
    for (const float y : {1.9F, 2.4F, 3.2F, 3.6F, 4.4F}) {
        const auto a = f.sample_extended(9.7F, y), b = h.sample_extended(9.7F, y);
        CHECK(a.u == b.u);
        CHECK(a.valid == b.valid);
        CHECK(a.level == b.level);
    }
}

TEST_CASE("thin rivers: a one-tile river at every zoom group stays calm, the shear fade works at "
          "its banks") {
    // A river one tile wide and 14 tiles long along the tile x axis with a parabolic velocity
    // profile (0.12 tiles/s at the banks, 0.92 in the middle), baked into the
    // lattice of its zoom group (strides 8 / 4 / 4 px) and shaded. At group 2 a
    // tile is 20 px: five texels across (rivers.md 13, Q10).
    const river::RiverConfig cfg;
    const auto params = cfg.params();
    for (std::size_t g = 0; g < render::kGroups; ++g) {
        const auto tw = static_cast<float>(render::kTile[g].w),
                   th = static_cast<float>(render::kTile[g].h);
        const std::uint32_t stride = cfg.lattice_stride[g];
        const render::RiverView v{tw, th, 2.059F * tw / 84.0F};
        river::FlowPlane plane;
        plane.stride_px = stride;
        plane.x0 = -static_cast<std::int32_t>(tw);
        plane.y0 = 0;
        plane.nx = static_cast<std::uint32_t>(18.0F * tw / static_cast<float>(stride)) + 2;
        plane.ny = static_cast<std::uint32_t>(18.0F * th / static_cast<float>(stride)) + 2;
        plane.tile_w = render::kTile[g].w;
        plane.tile_h = render::kTile[g].h;
        plane.uv.assign(std::size_t{plane.nx} * plane.ny * 2, 0);
        plane.aux.assign(std::size_t{plane.nx} * plane.ny * 4, 0);
        plane.shear.assign(std::size_t{plane.nx} * plane.ny, 0);
        auto tile_of = [&](float x, float y) {
            const float yw = y + v.lambda * 32.0F;
            return std::array<float, 2>{x / tw + yw / th, yw / th - x / tw};
        };
        for (std::uint32_t j = 0; j < plane.ny; ++j) {
            for (std::uint32_t i = 0; i < plane.nx; ++i) {
                const auto t = tile_of(static_cast<float>(plane.x0 + static_cast<int>(i * stride)),
                                       static_cast<float>(plane.y0 + static_cast<int>(j * stride)));
                if (t[1] >= -0.15F && t[1] < 1.15F && t[0] > 1.0F && t[0] < 15.0F) {
                    const float y = std::clamp(t[1], 0.0F, 1.0F);
                    // |du/dy| of the parabola, 1/s (the shear the island's flow
                    // would give across the channel).
                    const float shear = t[1] >= 0.0F && t[1] <= 1.0F
                                            ? std::fabs(0.8F * 4.0F * (1.0F - 2.0F * y))
                                            : 0.0F;
                    river::set_texel(plane, i, j, 0.12F + 0.8F * 4.0F * y * (1.0F - y), 0.0F, 1.0F,
                                     0.0F, 0.0F, 32.0F, 8.0F, shear);
                }
            }
        }
        const float detail = render::wave_detail(tw);
        // Pixels inside the river proper, split into bank and core bands.
        struct Px {
            int x, y;
            bool bank, core;
            render::FlowSample f;
        };
        std::vector<Px> pixels;
        float max_shear = 0;
        for (int y = plane.y0 + 4; y < plane.y0 + 17 * static_cast<int>(th); ++y) {
            for (int x = plane.x0 + 4; x < plane.x0 + 17 * static_cast<int>(tw); ++x) {
                const auto t = tile_of(static_cast<float>(x), static_cast<float>(y));
                if (t[1] < 0.0F || t[1] >= 1.0F || t[0] < 2.0F || t[0] > 14.0F) {
                    continue;
                }
                const render::FlowSample f = river::flow_at(plane, x, y);
                if (f.valid < 0.9F) {
                    continue;
                }
                max_shear = std::max(max_shear, f.shear);
                pixels.push_back({x, y, t[1] < 0.2F || t[1] > 0.8F, t[1] > 0.4F && t[1] < 0.6F, f});
            }
        }
        REQUIRE(pixels.size() > 250);
        CHECK(std::count_if(pixels.begin(), pixels.end(), [](const Px& p) { return p.bank; }) > 20);
        double lo = 1e9, hi = 0, mean = 0, bank_sd = 0, core_sd = 0;
        int n = 0;
        for (std::uint64_t t = 0; t < 2400; t += 300) {
            std::vector<double> all, bank, core;
            for (const Px& p : pixels) {
                const float l =
                    render::river_flow_shade(static_cast<float>(p.x), static_cast<float>(p.y), p.f,
                                             4.0F, 0.0F, t, detail, params, v)
                        .light;
                REQUIRE(std::isfinite(l));
                CHECK(std::fabs(l) <= 0.2F);
                all.push_back(static_cast<double>(l));
                (p.bank ? bank : core).push_back(static_cast<double>(l));
                if (!p.bank && !p.core) {
                    core.pop_back();
                }
            }
            const double sd = stat(all).sd;
            lo = std::min(lo, sd);
            hi = std::max(hi, sd);
            mean += sd;
            bank_sd += stat(bank).sd;
            core_sd += stat(core).sd;
            ++n;
        }
        mean /= n;
        INFO("group " << g << ": " << pixels.size() << " pixels, sd range " << lo << " .. " << hi
                      << ", bank sd " << bank_sd / n << ", core sd " << core_sd / n
                      << ", largest shear " << max_shear);
        // The shear at the banks is large enough to fade the layers (T |grad u|
        // above shear_from for the slow layer) ...
        CHECK(max_shear * 2.4F > cfg.shear_from);
        if (g < 2) {
            // No pulsing artefact: the contrast of the river never collapses or
            // flares over a cycle (a few hundred pixels limit the precision; the
            // 2 % law is the large-patch case above) ...
            CHECK((hi - lo) / mean < 0.5);
            // ... and the banks are calmer than the core.
            CHECK(bank_sd < core_sd);
        } else {
            // Group 2: a texel is 0.2-0.4 tile, as wide as the banks of a river one
            // tile wide, so the shear (a gradient of 0.9 tiles/s over half a tile)
            // is felt everywhere and the slow layers fade across the whole river:
            // it is calm, far below the legacy shimmer's 0.018.
            CHECK(hi < 0.006);
            CHECK(bank_sd / n < 0.006);
            CHECK(core_sd / n < 0.006);
        }
    }
}

namespace {

/// The noise coordinates of the first phase of `layer` at a pixel, the way
/// river_flow_shade forms them (water-plane position, then flow_phases).
std::array<float, 2> noise_coord(const render::RiverLayer& layer, const render::FlowSample& f,
                                 float px, float py, std::uint64_t t) {
    const render::RiverView v = view0();
    const float pyw = py + v.lambda * f.level;
    const std::array<float, 2> x{px / v.tw + pyw / v.th, pyw / v.th - px / v.tw};
    return render::flow_phases(x, {f.vx, f.vy}, t, layer, render::flow_psi(x)).c[0];
}

/// A flow that varies in space like a real river's: the speed changes by
/// about 0.25 tiles/s per tile (a shear of ~0.35 1/s, below the fade of the
/// layers; the lattice of the QA islands has 0.3-2 1/s) and the direction
/// turns, evaluated at a scene pixel.
render::FlowSample varying_flow(float px, float py) {
    const float a = 0.0075F * (px - 5000.0F) + 0.005F * (py - 9000.0F);
    render::FlowSample f;
    f.vx = 1.0F + 0.4F * std::sin(a);
    f.vy = 0.3F + 0.3F * std::cos(a * 1.3F);
    f.level = 32.0F;
    f.valid = 1.0F;
    return f;
}

} // namespace

TEST_CASE(
    "river flow shading: the noise coordinate follows the geometry where the flow varies, far "
    "from the origin") {
    // The pixels (5000, 9000) are ~275 tiles from the island origin along the
    // tile x axis. The coordinate of a pattern that is advected with the flow
    // changes by s per tile of x plus s T phi per tile/s of u, so between
    // neighbouring pixels it may differ from the geometric step s |dx| by the
    // change of u: well within a factor of two for a river's gradients. A
    // stretch along the flow measured from the island origin (the S5 bug)
    // multiplied the second term by the distance from the origin (~10x).
    const river::RiverConfig cfg;
    const render::RiverParams params = cfg.params();
    const render::RiverView v = view0();
    std::size_t total = 0, within = 0;
    float worst = 0;
    for (std::size_t l = 0; l < params.count; ++l) {
        const render::RiverLayer& layer = params.layers[l];
        for (const std::uint64_t t : {1000ULL, 1900ULL}) {
            for (int j = 0; j < 40; ++j) {
                for (int i = 0; i < 40; ++i) {
                    const float px = 5000.0F + static_cast<float>(i) * 7.0F;
                    const float py = 9000.0F + static_cast<float>(j) * 7.0F;
                    const auto c0 = noise_coord(layer, varying_flow(px, py), px, py, t);
                    const auto cx =
                        noise_coord(layer, varying_flow(px + 1.0F, py), px + 1.0F, py, t);
                    const auto cy =
                        noise_coord(layer, varying_flow(px, py + 1.0F), px, py + 1.0F, t);
                    // One pixel across / down is (1/tw, -1/tw) / (1/th, 1/th) tiles.
                    const float gx = layer.scale * std::sqrt(2.0F) / v.tw;
                    const float gy = layer.scale * std::sqrt(2.0F) / v.th;
                    const float rx = std::hypot(cx[0] - c0[0], cx[1] - c0[1]) / gx;
                    const float ry = std::hypot(cy[0] - c0[0], cy[1] - c0[1]) / gy;
                    for (const float r : {rx, ry}) {
                        ++total;
                        within += r <= 2.0F ? 1U : 0U;
                        worst = std::max(worst, r);
                    }
                }
            }
        }
    }
    INFO("share within 2x of the geometric step: "
         << static_cast<double>(within) / static_cast<double>(total) << ", worst ratio " << worst);
    CHECK(static_cast<double>(within) >= 0.95 * static_cast<double>(total));
}

TEST_CASE("river flow shading: light_gain scales the ripple layers and not the whitewater layer") {
    river::RiverConfig one;
    river::RiverConfig four;
    four.light_gain = 4.0F;
    const render::RiverParams a = one.params(), b = four.params();
    REQUIRE(a.count == b.count);
    int ripples = 0, rapids = 0;
    for (std::size_t l = 0; l < a.count; ++l) {
        if (a.layers[l].rapids != 0.0F) {
            CHECK(b.layers[l].amp == a.layers[l].amp);
            ++rapids;
        } else {
            CHECK(b.layers[l].amp == doctest::Approx(4.0F * a.layers[l].amp));
            ++ripples;
        }
        CHECK(b.layers[l].scale == a.layers[l].scale);
        CHECK(b.layers[l].period_ms == a.layers[l].period_ms);
    }
    CHECK(ripples >= 3);
    CHECK(rapids == 1);
    // And the light follows: four times the standard deviation on flat water.
    const double sd1 = stat(patch_light(a, uniform({0.5F, 0.2F}), 400, 100, 8)).sd;
    const double sd4 = stat(patch_light(b, uniform({0.5F, 0.2F}), 400, 100, 8)).sd;
    CHECK(sd4 == doctest::Approx(4.0 * sd1).epsilon(0.01));
    // A gain of zero leaves no ripple at all.
    river::RiverConfig zero;
    zero.light_gain = 0.0F;
    CHECK(stat(patch_light(zero.params(), uniform({0.5F, 0.2F}), 400, 100, 8)).sd == 0.0);
    // Negative or not finite: refused.
    river::RiverConfig bad;
    bad.light_gain = -1.0F;
    CHECK_FALSE(bad.validate(512, 3).empty());
    bad.light_gain = std::nanf("");
    CHECK_FALSE(bad.validate(512, 3).empty());
}

TEST_CASE(
    "river water plane per zoom group, and the legacy shimmer where the lattice has no flow") {
    namespace sea = render::sea;
    // E19: the lift of the water plane is 2.059 px per HEIGHT unit at the
    // original's tile width of 84 px and scales with the group's tile width
    // (written out here, independent of the production formula).
    for (std::size_t g = 0; g < render::kGroups; ++g) {
        const auto tw = static_cast<float>(render::kTile[g].w);
        const auto th = static_cast<float>(render::kTile[g].h);
        const render::RiverView v = sea::river_view(g, render::kHeightLift);
        INFO("group " << g);
        CHECK(v.tw == tw);
        CHECK(v.th == th);
        CHECK(v.lambda == doctest::Approx(2.059F * tw / 84.0F).epsilon(1e-6));
        CHECK(v.lambda > 0.0F);
        // Another lift scales the plane with it.
        const render::RiverView half = sea::river_view(g, {1000, 1000});
        CHECK(half.lambda == doctest::Approx(1.0F * tw / 84.0F).epsilon(1e-6));
    }
    const river::RiverConfig cfg;
    const render::RiverParams params = cfg.params();
    const sea::RiverShading shading{&params, cfg.min_valid};
    const render::RiverView v = sea::river_view(1, render::kHeightLift);
    const std::pair<float, float> pos{12.3F, 45.6F};
    const render::Shade legacy = render::river_shade(pos, 4.0F, 2.0F, 777);
    auto shade = [&](const sea::RiverShading* r, float valid) {
        render::FlowSample f = uniform({0.6F, -0.2F}, 32.0F, 0.1F);
        f.valid = valid;
        return sea::river_pixel_shade(r, f, 400, 900, 4.0F, 2.0F, 777, 0.7F, v, pos);
    };
    // Below min_valid (and with no shading at all, or no table): exactly the legacy shimmer.
    const sea::RiverShading no_table{nullptr, cfg.min_valid};
    for (const render::Shade s : {shade(&shading, cfg.min_valid * 0.9F), shade(&shading, 0.0F),
                                  shade(nullptr, 1.0F), shade(&no_table, 1.0F)}) {
        CHECK(s.light == legacy.light);
        CHECK(s.tint == legacy.tint);
        CHECK(s.foam == legacy.foam);
    }
    // At min_valid and above: the flow shading, exactly river_flow_shade.
    const render::FlowSample f = uniform({0.6F, -0.2F}, 32.0F, 0.1F);
    const render::Shade expect =
        render::river_flow_shade(400.0F, 900.0F, f, 4.0F, 2.0F, 777, 0.7F, params, v);
    for (const float valid : {cfg.min_valid, 0.5F, 1.0F}) {
        const render::Shade s = shade(&shading, valid);
        CHECK(s.light == expect.light);
        CHECK(s.tint == expect.tint);
    }
    CHECK(expect.light != legacy.light);
}

namespace {

struct ExtensionCheck {
    int checked = 0, wrong = 0, in_band = 0;
};

/// Checks sample_extended against the exact nearest wet cell at random points
/// of the box [bx0, bx1] x [by0, by1] (tiles) around a raster: valid, velocity
/// and level must equal the blend of sample() and the flow of a cell that is
/// within half a cell of the exact nearest (the sequential transform may pick
/// a neighbour of equal distance), fading with that distance.
ExtensionCheck check_extension(const river::RiverMask& m, float bx0, float bx1, float by0,
                               float by1, std::uint32_t seed) {
    river::RiverConfig cfg;
    cfg.log_fallback = false;
    const river::RiverFlow f = river::RiverFlow::build(m, cfg);
    const float cf = static_cast<float>(m.cells_per_tile());
    const auto cells = f.cells();
    REQUIRE(!cells.empty());
    auto rnd = [&seed] {
        seed = seed * 1664525U + 1013904223U;
        return static_cast<float>(seed >> 8) / 16777216.0F;
    };
    auto fade = [&](float d) {
        const float t = std::clamp(1.0F - d / cfg.extend_tiles, 0.0F, 1.0F);
        return t * t * (3.0F - 2.0F * t);
    };
    ExtensionCheck out;
    for (int k = 0; k < 5000; ++k) {
        const float tx = bx0 + rnd() * (bx1 - bx0), ty = by0 + rnd() * (by1 - by0);
        const auto s = f.sample(tx, ty);
        const auto e = f.sample_extended(tx, ty);
        float dtrue = 1e9F;
        for (const auto& c : cells) {
            const float dx = (static_cast<float>(c.cx) + 0.5F) / cf - tx;
            const float dy = (static_cast<float>(c.cy) + 0.5F) / cf - ty;
            dtrue = std::min(dtrue, std::sqrt(dx * dx + dy * dy));
        }
        if (s.valid >= 1.0F) {
            CHECK(e.valid == s.valid);
            continue;
        }
        // The reach is a count of cells (the transform measures between cells),
        // so a point within two cells of it may or may not be inside.
        if (std::fabs(dtrue - cfg.extend_tiles) < 2.0F / cf) {
            continue;
        }
        if (dtrue > cfg.extend_tiles) {
            // Beyond the reach: nothing but the part of the point that is wet.
            CHECK(e.valid == doctest::Approx(s.valid).epsilon(1e-6));
            continue;
        }
        bool matched = false;
        for (const auto& c : cells) {
            const float dx = (static_cast<float>(c.cx) + 0.5F) / cf - tx;
            const float dy = (static_cast<float>(c.cy) + 0.5F) / cf - ty;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d > dtrue + 0.5F / cf) {
                continue;
            }
            const float w = s.valid;
            const float valid = w + (1.0F - w) * fade(d);
            const float level = w * s.level + (1.0F - w) * c.level;
            const float ux = w * s.u[0] + (1.0F - w) * c.ux;
            const float uy = w * s.u[1] + (1.0F - w) * c.uy;
            if (std::fabs(e.valid - valid) < 2e-3F && std::fabs(e.level - level) < 2e-3F &&
                std::fabs(e.u[0] - ux) < 2e-3F && std::fabs(e.u[1] - uy) < 2e-3F) {
                matched = true;
                break;
            }
        }
        ++out.checked;
        out.wrong += matched ? 0 : 1;
        out.in_band += s.valid > 0.0F ? 1 : 0;
    }
    return out;
}

/// Level of a test cell: it changes across and along the shape, so that the
/// nearest cell shows in every field.
float test_level(std::int32_t x, std::int32_t y) {
    return 14.0F + static_cast<float>(x) * 0.05F + static_cast<float>(y) * 0.4F;
}

} // namespace

TEST_CASE(
    "river flow: the extension past the wet cells is the nearest wet cell's flow, fading with "
    "the true distance (ends, corners, diagonals, the fade band)") {
    // Three shapes, each against a brute-force reference at random points
    // around them: a straight channel with a sea strip behind it (ends and
    // corners), a diagonal strip (the nearest cell of a point beside a diagonal
    // edge needs the diagonal passes of the transform) and two blobs of
    // different size (the places where the nearest blob changes).
    {
        river::RiverMask m(8, 0, 0, 220, 60);
        for (std::int32_t y = 20; y < 28; ++y) {
            for (std::int32_t x = 8; x < 168; ++x) {
                m.set_wet(x, y, 4.0F, test_level(x, y));
            }
            for (std::int32_t x = 168; x < 176; ++x) {
                m.set_sea(x, y);
            }
        }
        const ExtensionCheck c = check_extension(m, -1.0F, 24.0F, 0.2F, 6.0F, 4242);
        MESSAGE("channel: " << c.checked << " points, " << c.wrong << " wrong, " << c.in_band
                            << " in the band");
        CHECK(c.checked > 1500);
        CHECK(c.in_band > 100);
        CHECK(c.wrong == 0);
    }
    {
        river::RiverMask m(8, 0, 0, 220, 220);
        for (std::int32_t y = 20; y < 200; ++y) {
            for (std::int32_t x = 20; x < 200; ++x) {
                if (std::abs((x - 20) - (y - 20) * 2 / 3) <= 3) { // a strip at ~34 degrees
                    m.set_wet(x, y, 4.0F, test_level(x, y));
                }
            }
        }
        const ExtensionCheck c = check_extension(m, 0.0F, 27.0F, 0.0F, 27.0F, 777);
        MESSAGE("diagonal strip: " << c.checked << " points, " << c.wrong << " wrong, " << c.in_band
                                   << " in the band");
        CHECK(c.checked > 800);
        CHECK(c.in_band > 50);
        CHECK(c.wrong <= 10); // < 1.5 % of the points: near-equal-distance neighbours
    }
    {
        river::RiverMask m(8, 0, 0, 260, 200);
        auto disk = [&](std::int32_t cx, std::int32_t cy, std::int32_t r) {
            for (std::int32_t y = cy - r; y <= cy + r; ++y) {
                for (std::int32_t x = cx - r; x <= cx + r; ++x) {
                    if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) {
                        m.set_wet(x, y, 4.0F, test_level(x, y));
                    }
                }
            }
        };
        disk(60, 60, 9);
        disk(140, 100, 14);
        const ExtensionCheck c = check_extension(m, 2.0F, 22.0F, 2.0F, 16.0F, 99);
        MESSAGE("two blobs: " << c.checked << " points, " << c.wrong << " wrong, " << c.in_band
                              << " in the band");
        CHECK(c.checked > 600);
        CHECK(c.in_band > 30);
        CHECK(c.wrong <= 10);
    }
    {
        // Scattered single cells and small squares: near-ties in every direction.
        river::RiverMask m(8, 0, 0, 200, 200);
        std::uint32_t seed = 31337;
        auto rnd = [&seed] {
            seed = seed * 1664525U + 1013904223U;
            return static_cast<std::int32_t>(seed >> 16);
        };
        for (int k = 0; k < 90; ++k) {
            const std::int32_t x = 20 + rnd() % 160, y = 20 + rnd() % 160;
            const std::int32_t size = 1 + rnd() % 4;
            for (std::int32_t dy = 0; dy < size; ++dy) {
                for (std::int32_t dx = 0; dx < size; ++dx) {
                    m.set_wet(x + dx, y + dy, 4.0F, test_level(x + dx, y + dy));
                }
            }
        }
        const ExtensionCheck c = check_extension(m, 3.0F, 22.0F, 3.0F, 22.0F, 5);
        MESSAGE("scattered cells: " << c.checked << " points, " << c.wrong << " wrong, "
                                    << c.in_band << " in the band");
        CHECK(c.checked > 1000);
        CHECK(c.wrong <= 40); // < 1 % of the points: near-equal-distance neighbours
    }
}

TEST_CASE("river configuration: the extension reach, the shear probe and the thread count are "
          "bounded") {
    river::RiverConfig c;
    CHECK(c.validate(512, 3).empty());
    c.extend_tiles = river::kMaxExtendTiles;
    CHECK(c.validate(512, 3).empty());
    c.extend_tiles = river::kMaxExtendTiles + 1.0F;
    CHECK_FALSE(c.validate(512, 3).empty());
    c.extend_tiles = 1.0e9F;
    CHECK_FALSE(c.validate(512, 3).empty());
    c.extend_tiles = -1.0F;
    CHECK_FALSE(c.validate(512, 3).empty());
    c = river::RiverConfig{};
    c.shear_probe_tiles = 0.0F;
    CHECK_FALSE(c.validate(512, 3).empty());
    c = river::RiverConfig{};
    c.flow_threads = 65;
    CHECK_FALSE(c.validate(512, 3).empty());
    c.flow_threads = 2;
    CHECK(c.validate(512, 3).empty());
}

namespace {

/// The default parameters at tier `q`.
render::RiverParams tier(render::RiverQuality q) {
    render::RiverParams p = river::RiverConfig{}.params();
    p.quality = q;
    return p;
}

/// The default table without the layers that fade with the zoom group's wave
/// detail (the glints and the whitewater streaks), at High: what Medium must
/// draw.
render::RiverParams without_fine_layers() {
    river::RiverConfig cfg;
    std::vector<river::LayerConfig> keep;
    for (const river::LayerConfig& l : cfg.layers) {
        if (!l.detail_fades) {
            keep.push_back(l);
        }
    }
    cfg.layers = keep;
    return cfg.params();
}

} // namespace

TEST_CASE("river quality: High is the reference look, Medium leaves out the layers that fade with "
          "the zoom group's detail") {
    const render::RiverParams high = tier(render::RiverQuality::High);
    const render::RiverParams medium = tier(render::RiverQuality::Medium);
    const render::RiverParams expected = without_fine_layers();
    CHECK(render::RiverParams{}.quality == render::RiverQuality::High);
    CHECK(high.quality == render::RiverQuality::High);
    std::size_t differ = 0, equal_rapids = 0, n = 0;
    for (int i = 0; i < 400; ++i) {
        const float px = 300.0F + 37.0F * static_cast<float>(i);
        const float py = 900.0F + 23.0F * static_cast<float>(i % 17);
        const std::uint64_t t = 137U * static_cast<std::uint64_t>(i);
        const render::FlowSample f = uniform({0.5F, -0.3F}, 32.0F, 0.2F);
        for (const float slope : {0.0F, 20.0F}) {
            const float m = light_at(medium, f, px, py, t, slope);
            const float h = light_at(high, f, px, py, t, slope);
            // Medium is exactly the table without the fine layers at High.
            // (With slope the whitewater layer is among the fine ones.)
            CHECK(m == light_at(expected, f, px, py, t, slope));
            ++n;
            differ += m != h ? 1U : 0U;
            // Without detail the fine layers have no amplitude: the tiers agree.
            const float m0 = light_at(medium, f, px, py, t, slope, 0.0F);
            const float h0 = light_at(high, f, px, py, t, slope, 0.0F);
            equal_rapids += m0 == h0 ? 1U : 0U;
        }
    }
    // The glints and streaks are part of High.
    CHECK(differ > n * 9 / 10);
    CHECK(equal_rapids == n);
}

TEST_CASE("river quality: Low takes one noise tap per phase and keeps the light variance of "
          "Medium") {
    const render::RiverParams medium = tier(render::RiverQuality::Medium);
    const render::RiverParams low = tier(render::RiverQuality::Low);
    // Same layers as Medium (the fine ones are out), a different light.
    {
        const render::FlowSample f = uniform({0.4F, 0.2F});
        bool differs = false;
        for (int i = 0; i < 50; ++i) {
            const float px = 300.0F + 91.0F * static_cast<float>(i);
            differs = differs ||
                      light_at(low, f, px, 900.0F, 500) != light_at(medium, f, px, 900.0F, 500);
        }
        CHECK(differs);
    }
    // The calibration of kLowTapGain: the standard deviation of Low within 12 %
    // of Medium (0.97 measured), over the speeds and directions of a river.
    for (const float speed : {0.12F, 0.5F, 0.9F}) {
        double sd_low = 0, sd_med = 0, mean_low = 0;
        int m = 0;
        for (const std::array<float, 2> d : {std::array<float, 2>{1.0F, 0.0F},
                                             {0.0F, 1.0F},
                                             {0.7071F, 0.7071F},
                                             {0.7071F, -0.7071F}}) {
            for (std::uint64_t t = 0; t < 2400; t += 400) {
                const render::FlowSample f = uniform({speed * d[0], speed * d[1]});
                const Stat a = stat(patch_light(low, f, t));
                sd_low += a.sd;
                mean_low += a.mean;
                sd_med += stat(patch_light(medium, f, t)).sd;
                ++m;
            }
        }
        sd_low /= m;
        sd_med /= m;
        mean_low /= m;
        MESSAGE("speed " << speed << ": sd low " << sd_low << ", medium " << sd_med);
        CHECK(sd_low / sd_med >= 0.88);
        CHECK(sd_low / sd_med <= 1.12);
        // The brightness does not drift: the noise is centred.
        CHECK(std::fabs(mean_low) < 0.003);
    }
    // Bounded light at every tier.
    for (const render::RiverQuality q :
         {render::RiverQuality::Low, render::RiverQuality::Medium, render::RiverQuality::High}) {
        const render::RiverParams p = tier(q);
        for (const double v : patch_light(p, uniform({0.9F, 0.0F}), 777, 60, 11)) {
            CHECK(std::isfinite(v));
            CHECK(std::fabs(v) < 0.2);
        }
    }
}
