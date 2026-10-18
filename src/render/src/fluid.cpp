// Port of crates/render/src/fluid.rs: every float expression keeps the Rust
// operand order (no contraction, -ffp-contract=off / /fp:strict), Rust's
// f32::max/min are fmaxf/fminf (NaN-ignoring), f32::clamp is clampf,
// float-to-integer `as` casts saturate (sat_cast, sat_i64).
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

#include <anno/render/fluid.hpp>
#include <anno/render/numeric.hpp>

namespace anno::render::fluid {

namespace {

/// Rust's f32 TAU (2π rounded to f32).
constexpr float kTau = 6.28318530717958647692F;

/// Rust `v as i64` for f64: truncated toward zero, saturated, NaN → 0.
std::int64_t sat_i64(double v) {
    if (std::isnan(v)) {
        return 0;
    }
    constexpr auto lo = std::numeric_limits<std::int64_t>::min();
    constexpr auto hi = std::numeric_limits<std::int64_t>::max();
    // −2^63 and 2^63 are exact doubles; 2^63 itself is out of range.
    if (v <= static_cast<double>(lo)) {
        return lo;
    }
    if (v >= static_cast<double>(hi)) {
        return hi;
    }
    return static_cast<std::int64_t>(v);
}

std::size_t to_size(std::int64_t v) {
    return static_cast<std::size_t>(v);
}

} // namespace

std::uint32_t FluidConfig::window_size(std::size_t group, float span_tiles) const {
    const float cells = span_tiles * static_cast<float>(cells_per_tile.at(group)) +
                        2.0F * static_cast<float>(recentre_cells + sponge_cells);
    const auto c = sat_cast<std::uint32_t>(std::ceil(cells));
    // u32::div_ceil(64) * 64 (wrapping as Rust release builds do). The step
    // of 64 cells is fluid.rs's (no reason given there; it is a multiple of
    // the shader's 8 × 8 workgroups and keeps the grid from being recreated
    // on every small resize). Configurable path: a FluidConfig field.
    const std::uint32_t n = (c / 64U + (c % 64U != 0 ? 1U : 0U)) * 64U;
    // Rust: n.clamp(64, size), which panics for size < 64.
    return std::min(std::max(n, 64U), size);
}

Swell swell_components(const FluidConfig& cfg) {
    std::uint32_t state = cfg.seed * 747'796'405U + 2'891'336'453U;
    auto rand = [&state] {
        state = state * 1'664'525U + 1'013'904'223U;
        return static_cast<float>(state >> 8) / 16'777'216.0F;
    };
    Swell out{};
    for (std::size_t n = 0; n < kSwellComponents; ++n) {
        const float base = static_cast<float>(n) / static_cast<float>(kSwellComponents) * kTau;
        // The order of the rand() calls is part of the result.
        const float theta = cfg.swell_dir + base + (rand() - 0.5F) * 0.6F;
        const float off = theta - cfg.swell_dir;
        // powi(2) is a multiplication.
        const float weight = 0.3F + 0.7F * sq(std::cos(off / 2.0F));
        const float amplitude = cfg.swell_amplitude * weight * (0.7F + 0.3F * rand());
        const float period =
            cfg.swell_periods.first + (cfg.swell_periods.second - cfg.swell_periods.first) * rand();
        const float omega = kTau / period;
        const float k = omega / cfg.deep_speed;
        out[n] = SwellComponent{
            .k = {k * std::cos(theta), k * std::sin(theta)},
            .omega = omega,
            .phase = rand() * kTau,
            .amplitude = amplitude,
        };
    }
    return out;
}

Forcing forcing(const FluidConfig& cfg, const Swell& comps, std::pair<float, float> pos, float t) {
    const auto [x, y] = pos;
    Forcing f;
    const float s = cfg.deep_speed / cfg.depth_cap;
    for (const SwellComponent& c : comps) {
        const float e = c.amplitude * std::cos(c.k[0] * x + c.k[1] * y - c.omega * t + c.phase);
        const float kn = fmaxf(std::sqrt(c.k[0] * c.k[0] + c.k[1] * c.k[1]), 1e-6F);
        f.eta += e;
        f.u += e * s * c.k[0] / kn;
        f.v += e * s * c.k[1] / kn;
    }
    return f;
}

FluidWindow FluidWindow::centred(const FluidConfig& cfg, std::size_t group,
                                 std::pair<float, float> centre) {
    return with_size(cfg, group, centre, cfg.size);
}

FluidWindow FluidWindow::with_size(const FluidConfig& cfg, std::size_t group,
                                   std::pair<float, float> centre, std::uint32_t size) {
    FluidWindow w;
    w.size = std::int64_t{size};
    w.cells_per_tile = cfg.cells_per_tile.at(group);
    w.origin = w.origin_for(centre);
    return w;
}

std::pair<std::int64_t, std::int64_t>
FluidWindow::origin_for(std::pair<float, float> centre) const {
    const double cpt = static_cast<double>(cells_per_tile);
    auto c = [&](float v) { return sat_i64(std::floor(static_cast<double>(v) * cpt)) - size / 2; };
    return {c(centre.first), c(centre.second)};
}

std::vector<CellRect> FluidWindow::recentre(std::pair<float, float> centre,
                                            std::uint32_t threshold) {
    const auto next = origin_for(centre);
    const std::int64_t dx = next.first - origin.first, dy = next.second - origin.second;
    const auto t = std::int64_t{threshold};
    if (std::abs(dx) < t && std::abs(dy) < t) {
        return {};
    }
    const CellRect old = rect();
    origin = next;
    const CellRect now = rect();
    if (std::abs(dx) >= size || std::abs(dy) >= size) {
        return {now};
    }
    std::vector<CellRect> out;
    // Columns that are new (full height of the new window) ...
    const auto xs = dx > 0 ? std::pair{old.x1, now.x1} : std::pair{now.x0, old.x0};
    if (xs.second > xs.first) {
        out.push_back({xs.first, now.y0, xs.second, now.y1});
    }
    // ... and rows that are new within the old columns.
    const std::int64_t cx0 = std::max(now.x0, old.x0), cx1 = std::min(now.x1, old.x1);
    const auto ys = dy > 0 ? std::pair{old.y1, now.y1} : std::pair{now.y0, old.y0};
    if (ys.second > ys.first && cx1 > cx0) {
        out.push_back({cx0, ys.first, cx1, ys.second});
    }
    return out;
}

std::pair<std::size_t, std::size_t> FluidWindow::storage(std::int64_t x, std::int64_t y) const {
    return {to_size(rem_euclid(x, size)), to_size(rem_euclid(y, size))};
}

std::pair<float, float> FluidWindow::cell_centre(std::int64_t x, std::int64_t y) const {
    const double cpt = static_cast<double>(cells_per_tile);
    auto p = [cpt](std::int64_t v) {
        return static_cast<float>((static_cast<double>(v) + 0.5) / cpt);
    };
    return {p(x), p(y)};
}

Cell reset_cell(const FluidConfig& cfg, const Swell& comps, const FluidWindow& window,
                std::pair<std::int64_t, std::int64_t> cell, float t, float h) {
    if (h <= kWet) {
        return {-h, 0.0F, 0.0F, 0.0F};
    }
    const Forcing f = forcing(cfg, comps, window.cell_centre(cell.first, cell.second), t);
    return {fmaxf(f.eta, -h), f.u, f.v, 0.0F};
}

FluidGrid::FluidGrid(FluidConfig cfg_, FluidWindow window_, const DepthFn& depth_fn, float t)
    : cfg(std::move(cfg_)), comps(swell_components(cfg)), window(window_) {
    const std::size_t n = to_size(window.size);
    depth.assign(n * n, 0.0F);
    state.assign(n * n, Cell{});
    fill(window.rect(), depth_fn, t);
}

void FluidGrid::fill(CellRect r, const DepthFn& depth_fn, float t) {
    const std::size_t n = to_size(window.size);
    for (std::int64_t y = r.y0; y < r.y1; ++y) {
        for (std::int64_t x = r.x0; x < r.x1; ++x) {
            const auto [sx, sy] = window.storage(x, y);
            const auto [px, py] = window.cell_centre(x, y);
            const float h = depth_fn(px, py);
            depth[sy * n + sx] = h;
            state[sy * n + sx] = reset_cell(cfg, comps, window, {x, y}, t, h);
        }
    }
}

float FluidGrid::force_factor(std::int64_t x, std::int64_t y, float h, float dt) const {
    const FluidConfig& c = cfg;
    const std::int64_t lx = x - window.origin.first, ly = y - window.origin.second;
    const std::int64_t edge =
        std::min(std::min(std::min(lx, ly), window.size - 1 - lx), window.size - 1 - ly);
    const float sponge =
        smoothstep(static_cast<float>(c.sponge_cells), 0.0F, static_cast<float>(edge));
    const float deep = smoothstep(c.force_depth.first, c.force_depth.second, h);
    return fmaxf(fminf(deep * c.force_rate * dt, 1.0F), sponge);
}

void FluidGrid::step(float t) {
    const FluidConfig& c = cfg;
    const std::size_t n = to_size(window.size);
    const float dt = c.dt;
    const float dx = 1.0F / static_cast<float>(window.cells_per_tile);
    const float g = c.gravity();
    const FluidWindow w = window;
    auto at = [&w, n](std::int64_t x, std::int64_t y) {
        const auto [sx, sy] = w.storage(x, y);
        return sy * n + sx;
    };
    const CellRect r = w.rect();
    // Pass A: velocities on faces; a face carries flow when the higher
    // surface lies above the higher bed (wet/dry).
    const std::vector<Cell> src = state;
    std::vector<Cell> mid = src;
    for (std::int64_t y = r.y0; y < r.y1; ++y) {
        for (std::int64_t x = r.x0; x < r.x1; ++x) {
            const std::size_t i = at(x, y);
            const float h = depth[i];
            // Named copies (no structured bindings: the lambda captures eta).
            const float eta = src[i][0], u0 = src[i][1], v0 = src[i][2], foam = src[i][3];
            auto face = [&](float vel, std::size_t j) -> float {
                const float s_r = src[j][0], b = fmaxf(-h, -depth[j]);
                const float d = fmaxf(eta, s_r) - b;
                if (d <= kWet) {
                    return 0.0F;
                }
                vel = vel - g * dt * (s_r - eta) / dx;
                vel /= 1.0F + dt * (c.damping + c.friction / fmaxf(d, c.min_depth));
                return clampf(vel, -c.max_speed, c.max_speed);
            };
            float u = face(u0, at(x + 1, y));
            float v = face(v0, at(x, y + 1));
            const float f = force_factor(x, y, h, dt);
            if (f > 0.0F && h > kWet) {
                const auto [px, py] = w.cell_centre(x, y);
                const float half = 0.5F * dx;
                const float fu = forcing(c, comps, {px + half, py}, t).u;
                const float fv = forcing(c, comps, {px, py + half}, t).v;
                u += (fu - u) * f;
                v += (fv - v) * f;
            }
            mid[i] = {eta, u, v, foam};
        }
    }
    // Pass B: elevation from upwind fluxes, dry floor, breaking, foam.
    std::vector<Cell> out = mid;
    const float decay = std::exp(-dt / c.foam_decay);
    auto total = [&](std::size_t j) { return clampf(mid[j][0] + depth[j], 0.0F, c.depth_cap); };
    for (std::int64_t y = r.y0; y < r.y1; ++y) {
        for (std::int64_t x = r.x0; x < r.x1; ++x) {
            const std::size_t i = at(x, y);
            const float h = depth[i];
            float eta = mid[i][0];
            const float u = mid[i][1], v = mid[i][2];
            const std::size_t l = at(x - 1, y), up = at(x, y - 1);
            const float ul = mid[l][1], vu = mid[up][2];
            // Foam drifts with the flow (semi-Lagrangian): sampled where the
            // water at this cell centre came from.
            const float cpt = static_cast<float>(window.cells_per_tile);
            const float bx = static_cast<float>(x) - 0.5F * (u + ul) * dt * cpt;
            const float by = static_cast<float>(y) - 0.5F * (v + vu) * dt * cpt;
            float foam = 0.0F;
            {
                const float fx0 = std::floor(bx), fy0 = std::floor(by);
                const float fx = bx - fx0, fy = by - fy0;
                const auto ix = sat_cast<std::int64_t>(fx0), iy = sat_cast<std::int64_t>(fy0);
                auto q = [&](std::int64_t a, std::int64_t b) { return mid[at(a, b)][3]; };
                foam = (q(ix, iy) * (1.0F - fx) + q(ix + 1, iy) * fx) * (1.0F - fy) +
                       (q(ix, iy + 1) * (1.0F - fx) + q(ix + 1, iy + 1) * fx) * fy;
            }
            auto flux = [&](float vel, std::size_t from, std::size_t to) {
                return vel * (vel > 0.0F ? total(from) : total(to));
            };
            const float div = (flux(u, i, at(x + 1, y)) - flux(ul, l, i)) +
                              (flux(v, i, at(x, y + 1)) - flux(vu, up, i));
            eta -= dt * div / dx;
            if (h <= 0.0F) {
                eta -= dt * c.infiltration;
            }
            eta = fmaxf(eta, -h);
            const float d = eta + h;
            if (h > c.break_min_depth) {
                const float lim = c.break_ratio * h;
                if (eta > lim) {
                    const float ex = (eta - lim) * fminf(c.break_rate * dt, 1.0F);
                    eta -= ex;
                    foam += ex * c.foam_gain;
                }
            } else if (d > kWet && h <= c.min_depth) {
                // Swash: turbulent sheet flow churns foam.
                const float speed =
                    0.5F * (std::abs(u) + std::abs(ul) + std::abs(v) + std::abs(vu));
                foam += dt * c.swash_foam * speed;
            }
            foam *= decay;
            foam = fmaxf(foam, 0.0F);
            const float f = force_factor(x, y, h, dt);
            if (f > 0.0F && h > kWet) {
                const auto [px, py] = w.cell_centre(x, y);
                const float fe = forcing(c, comps, {px, py}, t).eta;
                eta += (fmaxf(fe, -h) - eta) * f;
            }
            out[i] = {eta, u, v, foam};
        }
    }
    state = std::move(out);
}

float smoothstep(float a, float b, float x) {
    const float t = clampf((x - a) / (b - a), 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}

} // namespace anno::render::fluid
