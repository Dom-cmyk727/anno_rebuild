// RiverFlow (phase B): the steady depth-averaged flow of a river raster.
// docs/render/rivers.md 2.3-2.4 and 3.2 (R5-R8). Everything here is a pure,
// single-threaded function of the mask and the config; sums run in a fixed
// order, so the result does not depend on how many islands are built at once.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <queue>
#include <utility>

#include <anno/render/iso.hpp>
#include <anno/render/river.hpp>

namespace anno::render::river {

namespace {

// Face slots of a cell: -x, +x, -y, +y.
constexpr std::array<int, 4> kDx = {-1, 1, 0, 0};
constexpr std::array<int, 4> kDy = {0, 0, -1, 1};

// A uint8_t promotes to int. Shifting that int and masking with 1U is a sign
// conversion, and UBSan plus -Werror rejects it. The stored bits are 0..3.
bool sea_face_bit(std::uint8_t face, unsigned bit) {
    return ((static_cast<unsigned>(face) >> bit) & 1U) != 0;
}

/// A symmetric positive definite operator on a set of cells with up to four
/// neighbours each: (A x)_i = diag_i x_i - sum_d g_id x_nb(i, d), diag_i = sum_d g_id +
/// sink_i (sink: conductance to a ghost cell at potential 0, an outlet).
struct Operator {
    std::size_t n = 0;
    std::vector<std::int32_t> nb;   // 4 n, -1 = none
    std::vector<double> g;          // 4 n
    std::vector<double> sink, diag; // n
    std::vector<std::int32_t> cx, cy;

    void apply(const double* x, double* y) const {
        for (std::size_t i = 0; i < n; ++i) {
            double s = diag[i] * x[i];
            for (std::size_t d = 0; d < 4; ++d) {
                const std::int32_t j = nb[4 * i + d];
                if (j >= 0) {
                    s -= g[4 * i + d] * x[static_cast<std::size_t>(j)];
                }
            }
            y[i] = s;
        }
    }
    void finish() {
        diag.assign(n, 0.0);
        for (std::size_t i = 0; i < n; ++i) {
            double s = sink[i];
            for (std::size_t d = 0; d < 4; ++d) {
                s += g[4 * i + d];
            }
            diag[i] = s;
        }
    }
};

/// Coarse operator of 2 x 2 aggregates. Conductances between aggregates are
/// half the sum of the fine faces across (the distance doubles: for a channel
/// one cell wide the coarse step is two fine steps in series; for a wide
/// reach two faces in parallel at twice the distance), the same factor for
/// the outlet conductances. Only a preconditioner, so approximate is fine; it
/// stays symmetric and positive.
Operator coarsen(const Operator& f, std::vector<std::int32_t>& parent) {
    std::vector<std::pair<std::uint64_t, std::int32_t>> key(f.n);
    for (std::size_t i = 0; i < f.n; ++i) {
        const auto kx = static_cast<std::uint64_t>(f.cx[i] >> 1);
        const auto ky = static_cast<std::uint64_t>(f.cy[i] >> 1);
        key[i] = {ky << 32 | kx, static_cast<std::int32_t>(i)};
    }
    std::sort(key.begin(), key.end());
    parent.assign(f.n, 0);
    Operator c;
    std::int32_t id = -1;
    std::uint64_t last = ~std::uint64_t{0};
    for (const auto& [k, i] : key) {
        if (k != last) {
            ++id;
            last = k;
            c.cx.push_back(static_cast<std::int32_t>(k & 0xFFFFFFFFU));
            c.cy.push_back(static_cast<std::int32_t>(k >> 32));
        }
        parent[static_cast<std::size_t>(i)] = id;
    }
    c.n = static_cast<std::size_t>(id + 1);
    c.nb.assign(4 * c.n, -1);
    c.g.assign(4 * c.n, 0.0);
    c.sink.assign(c.n, 0.0);
    for (std::size_t i = 0; i < f.n; ++i) {
        const auto ci = static_cast<std::size_t>(parent[i]);
        c.sink[ci] += 0.5 * f.sink[i];
        for (std::size_t d = 0; d < 4; ++d) {
            const std::int32_t j = f.nb[4 * i + d];
            if (j < 0 || parent[static_cast<std::size_t>(j)] == parent[i]) {
                continue;
            }
            c.nb[4 * ci + d] = parent[static_cast<std::size_t>(j)];
            c.g[4 * ci + d] += 0.5 * f.g[4 * i + d];
        }
    }
    c.finish();
    return c;
}

/// Dense Cholesky factor of an operator (small coarsest levels).
struct Dense {
    std::size_t n = 0;
    std::vector<double> l;
    bool ok = false;

    void factor(const Operator& a) {
        n = a.n;
        l.assign(n * n, 0.0);
        for (std::size_t i = 0; i < n; ++i) {
            l[i * n + i] = a.diag[i];
            for (std::size_t d = 0; d < 4; ++d) {
                const std::int32_t j = a.nb[4 * i + d];
                if (j >= 0) {
                    l[i * n + static_cast<std::size_t>(j)] -= a.g[4 * i + d];
                }
            }
        }
        ok = true;
        for (std::size_t j = 0; j < n; ++j) {
            double s = l[j * n + j];
            for (std::size_t k = 0; k < j; ++k) {
                s -= l[j * n + k] * l[j * n + k];
            }
            if (!(s > 0.0) || !std::isfinite(s)) {
                ok = false;
                return;
            }
            const double d = std::sqrt(s);
            l[j * n + j] = d;
            for (std::size_t i = j + 1; i < n; ++i) {
                double t = l[i * n + j];
                for (std::size_t k = 0; k < j; ++k) {
                    t -= l[i * n + k] * l[j * n + k];
                }
                l[i * n + j] = t / d;
            }
        }
    }
    void solve(const double* b, double* x) const {
        for (std::size_t i = 0; i < n; ++i) {
            double s = b[i];
            for (std::size_t k = 0; k < i; ++k) {
                s -= l[i * n + k] * x[k];
            }
            x[i] = s / l[i * n + i];
        }
        for (std::size_t i = n; i-- > 0;) {
            double s = x[i];
            for (std::size_t k = i + 1; k < n; ++k) {
                s -= l[k * n + i] * x[k];
            }
            x[i] = s / l[i * n + i];
        }
    }
};

/// PCG with a symmetric V(1,1)-cycle of damped Jacobi smoothing over
/// aggregation levels as the preconditioner.
class Solver {
  public:
    Solver(Operator fine, const RiverConfig& cfg) {
        levels_.emplace_back();
        levels_.back().a = std::move(fine);
        while (levels_.size() < cfg.max_levels && levels_.back().a.n > cfg.coarsest_cells) {
            std::vector<std::int32_t> parent;
            Operator c = coarsen(levels_.back().a, parent);
            // Stagnation (no real reduction): stop and solve here.
            if (c.n * 20 > levels_.back().a.n * 19) {
                break;
            }
            levels_.back().parent = std::move(parent);
            levels_.emplace_back();
            levels_.back().a = std::move(c);
        }
        for (Level& l : levels_) {
            const std::size_t n = l.a.n;
            l.inv.resize(n);
            for (std::size_t i = 0; i < n; ++i) {
                l.inv[i] = 1.0 / l.a.diag[i];
            }
            l.r.assign(n, 0.0);
            l.z.assign(n, 0.0);
            l.t.assign(n, 0.0);
        }
        sweeps_ = cfg.coarse_sweeps;
        const Operator& last = levels_.back().a;
        if (last.n <= cfg.dense_limit) {
            dense_.factor(last);
        }
    }

    [[nodiscard]] std::size_t levels() const { return levels_.size(); }

    /// Solves A x = b; returns the iterations used (cap + 1 if not converged).
    std::uint32_t solve(const std::vector<double>& b, std::vector<double>& x, double tol,
                        std::uint32_t max_iter) {
        const Operator& a = levels_.front().a;
        const std::size_t n = a.n;
        x.assign(n, 0.0);
        std::vector<double> r = b, z(n), p(n), ap(n);
        const double bn = std::sqrt(dot(b, b));
        if (bn == 0.0) {
            return 0;
        }
        precondition(r, z);
        p = z;
        double rz = dot(r, z);
        for (std::uint32_t k = 1; k <= max_iter; ++k) {
            a.apply(p.data(), ap.data());
            const double pap = dot(p, ap);
            if (!(pap > 0.0)) {
                return max_iter + 1;
            }
            const double alpha = rz / pap;
            for (std::size_t i = 0; i < n; ++i) {
                x[i] += alpha * p[i];
                r[i] -= alpha * ap[i];
            }
            if (std::sqrt(dot(r, r)) <= tol * bn) {
                return k;
            }
            precondition(r, z);
            const double rz_new = dot(r, z);
            const double beta = rz_new / rz;
            rz = rz_new;
            for (std::size_t i = 0; i < n; ++i) {
                p[i] = z[i] + beta * p[i];
            }
        }
        return max_iter + 1;
    }

    /// |b - A x| / |b|.
    double residual(const std::vector<double>& b, const std::vector<double>& x) const {
        const Operator& a = levels_.front().a;
        std::vector<double> ax(a.n);
        a.apply(x.data(), ax.data());
        double num = 0.0, den = 0.0;
        for (std::size_t i = 0; i < a.n; ++i) {
            num += (b[i] - ax[i]) * (b[i] - ax[i]);
            den += b[i] * b[i];
        }
        return den > 0.0 ? std::sqrt(num / den) : 0.0;
    }

  private:
    struct Level {
        Operator a;
        std::vector<std::int32_t> parent; // fine -> coarse (not on the last level)
        std::vector<double> inv, r, z, t;
    };
    /// Damping of the Jacobi smoothers: 2/3, the classical choice for the 5-point
    /// Laplacian (it minimises the smoothing factor of the high frequencies, 1/3;
    /// Briggs, Henson & McCormick, A Multigrid Tutorial, 2nd ed., 2000, 2.4).
    static constexpr double kOmega = 2.0 / 3.0;

    static double dot(const std::vector<double>& a, const std::vector<double>& b) {
        double s = 0.0;
        for (std::size_t i = 0; i < a.size(); ++i) {
            s += a[i] * b[i];
        }
        return s;
    }

    void precondition(const std::vector<double>& r, std::vector<double>& z) {
        levels_[0].r = r;
        cycle(0);
        z = levels_[0].z;
    }

    void cycle(std::size_t l) {
        Level& L = levels_[l];
        const Operator& a = L.a;
        const std::size_t n = a.n;
        if (l + 1 == levels_.size()) {
            if (dense_.ok) {
                dense_.solve(L.r.data(), L.z.data());
            } else {
                // No exact solve (a large stagnating level): damped Jacobi
                // sweeps, a symmetric polynomial in D^-1 A.
                std::fill(L.z.begin(), L.z.end(), 0.0);
                for (std::uint32_t sweep = 0; sweep < sweeps_; ++sweep) {
                    a.apply(L.z.data(), L.t.data());
                    for (std::size_t i = 0; i < n; ++i) {
                        L.z[i] += kOmega * L.inv[i] * (L.r[i] - L.t[i]);
                    }
                }
            }
            return;
        }
        for (std::size_t i = 0; i < n; ++i) {
            L.z[i] = kOmega * L.inv[i] * L.r[i];
        }
        a.apply(L.z.data(), L.t.data());
        Level& C = levels_[l + 1];
        std::fill(C.r.begin(), C.r.end(), 0.0);
        for (std::size_t i = 0; i < n; ++i) {
            C.r[static_cast<std::size_t>(L.parent[i])] += L.r[i] - L.t[i];
        }
        cycle(l + 1);
        for (std::size_t i = 0; i < n; ++i) {
            L.z[i] += C.z[static_cast<std::size_t>(L.parent[i])];
        }
        a.apply(L.z.data(), L.t.data());
        for (std::size_t i = 0; i < n; ++i) {
            L.z[i] += kOmega * L.inv[i] * (L.r[i] - L.t[i]);
        }
    }

    std::vector<Level> levels_;
    Dense dense_;
    std::uint32_t sweeps_ = 60;
};

struct Net {
    // Wet cells in raster order.
    std::size_t n = 0;
    std::vector<std::int32_t> cx, cy;   // island cell coordinates
    std::vector<std::int32_t> nb;       // 4 n, index of the wet neighbour or -1
    std::vector<std::uint8_t> sea_face; // n, bit d = the face in slot d borders sea
    std::vector<float> level, bed;      // per cell
    std::vector<std::uint8_t> flags;    // mask flags
    std::vector<float> h;               // hydraulic depth (tiles)
    std::vector<double> K;              // conveyance
    std::vector<float> dbank;           // distance to the nearest bank (cells)
    std::vector<float> slope;           // |grad level| (HEIGHT units per tile)
    std::vector<std::int32_t> comp;     // river index or -1 (pond)
};

/// Fast marching of |grad D| = 1 (first order, unit cells) from the seeds
/// (D given there), over the cells `cells` of the net; D in cells.
void eikonal(const Net& net, const std::vector<std::int32_t>& cells,
             const std::vector<std::int32_t>& local, const std::vector<double>& seed,
             std::vector<double>& dist) {
    const std::size_t m = cells.size();
    constexpr double kFar = 1e30;
    dist.assign(m, kFar);
    std::vector<std::uint8_t> done(m, 0);
    using Item = std::pair<double, std::int32_t>;
    std::priority_queue<Item, std::vector<Item>, std::greater<>> heap;
    for (std::size_t i = 0; i < m; ++i) {
        if (seed[i] < kFar) {
            dist[i] = seed[i];
            heap.emplace(seed[i], static_cast<std::int32_t>(i));
        }
    }
    auto update = [&](std::size_t i) {
        double a = kFar, b = kFar;
        for (std::size_t d = 0; d < 4; ++d) {
            const std::int32_t j = net.nb[4 * static_cast<std::size_t>(cells[i]) + d];
            if (j < 0) {
                continue;
            }
            const auto lj = static_cast<std::size_t>(local[static_cast<std::size_t>(j)]);
            if (!done[lj]) {
                continue;
            }
            if (d < 2) {
                a = std::min(a, dist[lj]);
            } else {
                b = std::min(b, dist[lj]);
            }
        }
        double t = kFar;
        if (a >= kFar) {
            t = b + 1.0;
        } else if (b >= kFar) {
            t = a + 1.0;
        } else if (std::fabs(a - b) >= 1.0) {
            t = std::min(a, b) + 1.0;
        } else {
            t = 0.5 * (a + b + std::sqrt(2.0 - (a - b) * (a - b)));
        }
        return t;
    };
    while (!heap.empty()) {
        const auto [t, ii] = heap.top();
        heap.pop();
        const auto i = static_cast<std::size_t>(ii);
        if (done[i] || t > dist[i]) {
            continue;
        }
        done[i] = 1;
        for (std::size_t d = 0; d < 4; ++d) {
            const std::int32_t j = net.nb[4 * static_cast<std::size_t>(cells[i]) + d];
            if (j < 0) {
                continue;
            }
            const auto lj = static_cast<std::size_t>(local[static_cast<std::size_t>(j)]);
            if (done[lj]) {
                continue;
            }
            const double nt = update(lj);
            if (nt < dist[lj]) {
                dist[lj] = nt;
                heap.emplace(nt, static_cast<std::int32_t>(lj));
            }
        }
    }
}

} // namespace

std::size_t RiverFlow::cell_at(std::int32_t cx, std::int32_t cy) const {
    const std::int64_t x = std::int64_t{cx} - x0_, y = std::int64_t{cy} - y0_;
    if (x < 0 || y < 0 || x >= nx_ || y >= ny_) {
        return npos;
    }
    const std::int32_t id = grid_[static_cast<std::size_t>(y) * nx_ + static_cast<std::size_t>(x)];
    return id < 0 ? npos : static_cast<std::size_t>(id);
}

void RiverFlow::build_extension(float extend_tiles) {
    extend_tiles_ = extend_tiles;
    extend_cells_ = static_cast<std::int32_t>(std::ceil(extend_tiles * static_cast<float>(c_)));
    ext_.clear();
    if (extend_cells_ <= 0 || cells_.empty()) {
        return;
    }
    // The nearest wet cell of every cell within extend_cells_ of the wet ones
    // (rivers.md R9): the 8-point sequential Euclidean distance transform
    // (Danielsson's two-pass vector propagation) over the bounding box grown
    // by that margin, carrying the id of the nearest source instead of its
    // offset. Deterministic (fixed scan order); nearest to within a cell.
    ext_x0_ = x0_ - extend_cells_;
    ext_y0_ = y0_ - extend_cells_;
    ext_nx_ = nx_ + 2 * static_cast<std::uint32_t>(extend_cells_);
    ext_ny_ = ny_ + 2 * static_cast<std::uint32_t>(extend_cells_);
    const auto w = static_cast<std::int32_t>(ext_nx_), h = static_cast<std::int32_t>(ext_ny_);
    constexpr std::int32_t kFar = std::numeric_limits<std::int32_t>::max();
    ext_.assign(std::size_t{ext_nx_} * ext_ny_, -1);
    std::vector<std::int32_t> d2(ext_.size(), kFar);
    for (std::size_t k = 0; k < cells_.size(); ++k) {
        const std::size_t at = static_cast<std::size_t>(cells_[k].cy - ext_y0_) * ext_nx_ +
                               static_cast<std::size_t>(cells_[k].cx - ext_x0_);
        ext_[at] = static_cast<std::int32_t>(k);
        d2[at] = 0;
    }
    // Cell (x, y) takes the source of its neighbour (nx, ny) when that one is nearer.
    auto relax = [&](std::int32_t x, std::int32_t y, std::int32_t nx, std::int32_t ny) {
        if (nx < 0 || ny < 0 || nx >= w || ny >= h) {
            return;
        }
        const std::int32_t src =
            ext_[static_cast<std::size_t>(ny) * ext_nx_ + static_cast<std::size_t>(nx)];
        if (src < 0) {
            return;
        }
        const Cell& c = cells_[static_cast<std::size_t>(src)];
        const std::int32_t dx = c.cx - ext_x0_ - x, dy = c.cy - ext_y0_ - y;
        const std::int32_t dd = dx * dx + dy * dy;
        const std::size_t at = static_cast<std::size_t>(y) * ext_nx_ + static_cast<std::size_t>(x);
        if (dd < d2[at]) {
            d2[at] = dd;
            ext_[at] = src;
        }
    };
    for (std::int32_t y = 0; y < h; ++y) {
        for (std::int32_t x = 0; x < w; ++x) {
            relax(x, y, x - 1, y);
            relax(x, y, x, y - 1);
            relax(x, y, x - 1, y - 1);
            relax(x, y, x + 1, y - 1);
        }
        for (std::int32_t x = w - 1; x >= 0; --x) {
            relax(x, y, x + 1, y);
        }
    }
    for (std::int32_t y = h - 1; y >= 0; --y) {
        for (std::int32_t x = w - 1; x >= 0; --x) {
            relax(x, y, x + 1, y);
            relax(x, y, x, y + 1);
            relax(x, y, x + 1, y + 1);
            relax(x, y, x - 1, y + 1);
        }
        for (std::int32_t x = 0; x < w; ++x) {
            relax(x, y, x - 1, y);
        }
    }
    // Beyond the reach there is no extension.
    const std::int64_t reach2 = std::int64_t{extend_cells_} * extend_cells_;
    for (std::size_t i = 0; i < ext_.size(); ++i) {
        if (d2[i] != kFar && std::int64_t{d2[i]} > reach2) {
            ext_[i] = -1;
        }
    }
}

RiverFlow::Sample RiverFlow::sample(float tx, float ty) const {
    const float cf = static_cast<float>(c_);
    const float fx = tx * cf - 0.5F, fy = ty * cf - 0.5F;
    const float flx = std::floor(fx), fly = std::floor(fy);
    const float a = fx - flx, b = fy - fly;
    const auto ix = static_cast<std::int32_t>(flx), iy = static_cast<std::int32_t>(fly);
    Sample s;
    float sum = 0.0F, ux = 0.0F, uy = 0.0F, level = 0.0F;
    for (int dy = 0; dy < 2; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
            const std::size_t k = cell_at(ix + dx, iy + dy);
            if (k == npos) {
                continue;
            }
            const float w = (dx != 0 ? a : 1.0F - a) * (dy != 0 ? b : 1.0F - b);
            sum += w;
            ux += w * cells_[k].ux;
            uy += w * cells_[k].uy;
            level += w * cells_[k].level;
        }
    }
    if (sum > 0.0F) {
        s.u = {ux / sum, uy / sum};
        s.level = level / sum;
    }
    s.valid = sum;
    return s;
}

RiverFlow::Sample RiverFlow::sample_extended(float tx, float ty) const {
    Sample s = sample(tx, ty);
    if (s.valid >= 1.0F || ext_.empty()) {
        return s;
    }
    const float cf = static_cast<float>(c_);
    // The cell containing the point, and the nearest wet cell recorded there.
    const std::int64_t cx = static_cast<std::int64_t>(std::floor(tx * cf));
    const std::int64_t cy = static_cast<std::int64_t>(std::floor(ty * cf));
    const std::int64_t x = cx - ext_x0_, y = cy - ext_y0_;
    if (x < 0 || y < 0 || x >= ext_nx_ || y >= ext_ny_) {
        return s;
    }
    const std::int32_t k =
        ext_[static_cast<std::size_t>(y) * ext_nx_ + static_cast<std::size_t>(x)];
    if (k < 0) {
        return s;
    }
    const Cell& n = cells_[static_cast<std::size_t>(k)];
    const float dx = (static_cast<float>(n.cx) + 0.5F) / cf - tx;
    const float dy = (static_cast<float>(n.cy) + 0.5F) / cf - ty;
    const float d = std::sqrt(dx * dx + dy * dy);
    // 1 at the cell, 0 at the reach.
    const float fade = smoothstep(extend_tiles_, 0.0F, d);
    const float w = s.valid;
    Sample out;
    out.u = {w * s.u[0] + (1.0F - w) * n.ux, w * s.u[1] + (1.0F - w) * n.uy};
    out.level = w * s.level + (1.0F - w) * n.level;
    out.valid = w + (1.0F - w) * fade;
    return out;
}

RiverFlow RiverFlow::build(const RiverMask& mask, const RiverConfig& cfg) {
    const auto t_start = std::chrono::steady_clock::now();
    RiverFlow out;
    out.c_ = mask.cells_per_tile();
    FlowStats& st = out.stats_;
    const double c = static_cast<double>(std::max(mask.cells_per_tile(), 1U));
    const double delta = 1.0 / c; // tiles per cell

    // --- the net: wet cells in raster order and their neighbours ---
    Net net;
    constexpr std::int32_t kMax = std::numeric_limits<std::int32_t>::max();
    constexpr std::int32_t kMin = std::numeric_limits<std::int32_t>::min();
    std::int32_t bx0 = kMax, by0 = kMax, bx1 = kMin, by1 = kMin;
    for (std::uint32_t y = 0; y < mask.ny(); ++y) {
        for (std::uint32_t x = 0; x < mask.nx(); ++x) {
            if ((mask.flags[std::size_t{y} * mask.nx() + x] & kWet) == 0) {
                continue;
            }
            const auto cx = mask.x0() + static_cast<std::int32_t>(x);
            const auto cy = mask.y0() + static_cast<std::int32_t>(y);
            bx0 = std::min(bx0, cx), bx1 = std::max(bx1, cx);
            by0 = std::min(by0, cy), by1 = std::max(by1, cy);
            net.cx.push_back(cx);
            net.cy.push_back(cy);
        }
    }
    net.n = net.cx.size();
    st.wet_cells = net.n;
    if (net.n == 0) {
        st.build_ms = 0;
        return out;
    }
    out.x0_ = bx0;
    out.y0_ = by0;
    out.nx_ = static_cast<std::uint32_t>(bx1 - bx0 + 1);
    out.ny_ = static_cast<std::uint32_t>(by1 - by0 + 1);
    out.grid_.assign(std::size_t{out.nx_} * out.ny_, -1);
    for (std::size_t i = 0; i < net.n; ++i) {
        out.grid_[static_cast<std::size_t>(net.cy[i] - by0) * out.nx_ +
                  static_cast<std::size_t>(net.cx[i] - bx0)] = static_cast<std::int32_t>(i);
    }
    auto id_at = [&](std::int32_t cx, std::int32_t cy) -> std::int32_t {
        if (cx < bx0 || cx > bx1 || cy < by0 || cy > by1) {
            return -1;
        }
        return out.grid_[static_cast<std::size_t>(cy - by0) * out.nx_ +
                         static_cast<std::size_t>(cx - bx0)];
    };
    net.nb.assign(4 * net.n, -1);
    net.sea_face.assign(net.n, 0);
    net.level.resize(net.n);
    net.bed.resize(net.n);
    net.flags.resize(net.n);
    for (std::size_t i = 0; i < net.n; ++i) {
        const std::size_t mi = mask.index(net.cx[i], net.cy[i]);
        net.level[i] = mask.level[mi];
        net.bed[i] = mask.bed[mi];
        net.flags[i] = mask.flags[mi];
        for (std::size_t d = 0; d < 4; ++d) {
            const std::int32_t nx = net.cx[i] + kDx[d], ny = net.cy[i] + kDy[d];
            net.nb[4 * i + d] = id_at(nx, ny);
            if (net.nb[4 * i + d] < 0 && (mask.flags_at(nx, ny) & kSea) != 0) {
                net.sea_face[i] = static_cast<std::uint8_t>(net.sea_face[i] | (1U << d));
                ++st.outlet_faces;
            }
        }
    }

    // --- hydraulic depth: the clamped carved depth, smoothed over the wet
    // cells (masked: dry cells and banks conduct nothing), then clamped. The
    // quantised painted bed (4-unit steps, 3 x 3 blocks) must not become
    // blocky speed through h^(5/3) (rivers.md G4, F-j). Diffusion with the
    // stable step 1/4 for ceil(2 sigma^2) passes is a Gaussian of width sigma
    // (cells) with no-flux boundaries.
    net.h.resize(net.n);
    {
        std::vector<float> cur(net.n), nxt(net.n);
        for (std::size_t i = 0; i < net.n; ++i) {
            const float d0 = net.level[i] - net.bed[i];
            cur[i] = std::isnan(d0) ? cfg.h_min : std::clamp(d0, 0.0F, cfg.h_max);
        }
        const double sigma = static_cast<double>(cfg.bed_sigma_tiles) * c;
        const auto passes = static_cast<std::uint32_t>(std::min(
            std::ceil(2.0 * sigma * sigma), static_cast<double>(cfg.bed_smoothing_max_passes)));
        for (std::uint32_t p = 0; p < passes; ++p) {
            for (std::size_t i = 0; i < net.n; ++i) {
                float acc = 0.0F;
                for (std::size_t d = 0; d < 4; ++d) {
                    const std::int32_t j = net.nb[4 * i + d];
                    if (j >= 0) {
                        acc += cur[static_cast<std::size_t>(j)] - cur[i];
                    }
                }
                nxt[i] = cur[i] + 0.25F * acc;
            }
            std::swap(cur, nxt);
        }
        for (std::size_t i = 0; i < net.n; ++i) {
            net.h[i] = kUnitTiles * std::clamp(cur[i], cfg.h_min, cfg.h_max);
        }
    }

    // --- distance to the nearest bank (cells): chamfer over the wet cells;
    // a bank is a dry neighbour that is not sea (sea is an opening).
    net.dbank.assign(net.n, 1e9F);
    {
        auto bank_at = [&](std::int32_t cx, std::int32_t cy) {
            return id_at(cx, cy) < 0 && (mask.flags_at(cx, cy) & kSea) == 0;
        };
        for (std::size_t i = 0; i < net.n; ++i) {
            float best = 1e9F;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if ((dx != 0 || dy != 0) && bank_at(net.cx[i] + dx, net.cy[i] + dy)) {
                        best = std::min(best, (dx != 0 && dy != 0) ? 1.41421356F : 1.0F);
                    }
                }
            }
            net.dbank[i] = best;
        }
        auto relax = [&](std::size_t i, std::int32_t dx, std::int32_t dy, float step) {
            const std::int32_t j = id_at(net.cx[i] + dx, net.cy[i] + dy);
            if (j >= 0) {
                net.dbank[i] =
                    std::min(net.dbank[i], net.dbank[static_cast<std::size_t>(j)] + step);
            }
        };
        for (std::size_t i = 0; i < net.n; ++i) {
            relax(i, -1, 0, 1.0F);
            relax(i, 0, -1, 1.0F);
            relax(i, -1, -1, 1.41421356F);
            relax(i, 1, -1, 1.41421356F);
        }
        for (std::size_t i = net.n; i-- > 0;) {
            relax(i, 1, 0, 1.0F);
            relax(i, 0, 1, 1.0F);
            relax(i, 1, 1, 1.41421356F);
            relax(i, -1, 1, 1.41421356F);
        }
    }

    // --- conveyance K = k_M h^e / nu(d_bank) (rivers.md E1, E4, E6) ---
    const double k_m = 1.0 / (static_cast<double>(cfg.manning_n) *
                              std::cbrt(static_cast<double>(cfg.metres_per_tile)));
    net.K.resize(net.n);
    for (std::size_t i = 0; i < net.n; ++i) {
        const double d_tiles = std::max(static_cast<double>(net.dbank[i]) - 0.5, 0.0) * delta;
        const double nu = 1.0 + static_cast<double>(cfg.bank_roughness) *
                                    std::exp(-d_tiles / static_cast<double>(cfg.bank_width_tiles));
        net.K[i] =
            k_m * std::pow(static_cast<double>(net.h[i]), static_cast<double>(cfg.conveyance_exp)) /
            nu;
    }

    // --- surface slope |grad Z| from the level raster (central over +-0.5 tile) ---
    net.slope.assign(net.n, 0.0F);
    {
        const std::int32_t k = std::max<std::int32_t>(1, static_cast<std::int32_t>(c) / 2);
        auto lev = [&](std::int32_t cx, std::int32_t cy) {
            const std::size_t mi = mask.index(cx, cy);
            return mi == RiverMask::npos ? std::numeric_limits<float>::quiet_NaN() : mask.level[mi];
        };
        for (std::size_t i = 0; i < net.n; ++i) {
            const float z0 = net.level[i];
            if (std::isnan(z0)) {
                continue;
            }
            float g[2] = {0.0F, 0.0F};
            for (int axis = 0; axis < 2; ++axis) {
                float a = lev(net.cx[i] - (axis == 0 ? k : 0), net.cy[i] - (axis == 1 ? k : 0));
                float b = lev(net.cx[i] + (axis == 0 ? k : 0), net.cy[i] + (axis == 1 ? k : 0));
                float span = 2.0F * static_cast<float>(k);
                if (std::isnan(a)) {
                    a = z0;
                    span -= static_cast<float>(k);
                }
                if (std::isnan(b)) {
                    b = z0;
                    span -= static_cast<float>(k);
                }
                g[axis] = span > 0.0F ? (b - a) / (span / static_cast<float>(c)) : 0.0F;
            }
            net.slope[i] = std::sqrt(g[0] * g[0] + g[1] * g[1]);
        }
    }

    // --- rivers: 4-connected pieces of water ---
    net.comp.assign(net.n, -1);
    std::vector<std::vector<std::int32_t>> rivers;
    std::vector<std::int32_t> pond_cells_list;
    {
        std::vector<std::int32_t> stack, members;
        std::vector<std::uint8_t> seen(net.n, 0);
        for (std::size_t s = 0; s < net.n; ++s) {
            if (seen[s]) {
                continue;
            }
            members.clear();
            stack.assign(1, static_cast<std::int32_t>(s));
            seen[s] = 1;
            while (!stack.empty()) {
                const auto i = static_cast<std::size_t>(stack.back());
                stack.pop_back();
                members.push_back(static_cast<std::int32_t>(i));
                for (std::size_t d = 0; d < 4; ++d) {
                    const std::int32_t j = net.nb[4 * i + d];
                    if (j >= 0 && !seen[static_cast<std::size_t>(j)]) {
                        seen[static_cast<std::size_t>(j)] = 1;
                        stack.push_back(j);
                    }
                }
            }
            std::sort(members.begin(), members.end());
            if (members.size() < cfg.pond_cells) {
                pond_cells_list.insert(pond_cells_list.end(), members.begin(), members.end());
                ++st.ponds;
            } else {
                for (const std::int32_t i : members) {
                    net.comp[static_cast<std::size_t>(i)] =
                        static_cast<std::int32_t>(rivers.size());
                }
                rivers.push_back(members);
            }
        }
    }
    st.components = rivers.size();

    // --- per river: potential, fluxes, velocities ---
    std::vector<float> ux(net.n, 0.0F), uy(net.n, 0.0F), dgx(net.n, 0.0F), dgy(net.n, 0.0F);
    std::vector<std::uint8_t> cell_flags(net.n, 0);
    std::vector<std::int32_t> local(net.n, -1);
    std::size_t down_good = 0, down_total = 0, down_good_sea = 0, down_total_sea = 0;
    std::size_t solved_cells = 0, floor_cells = 0, cap_cells = 0;
    std::size_t largest = 0;
    for (std::size_t ri = 0; ri < rivers.size(); ++ri) {
        const std::vector<std::int32_t>& cells = rivers[ri];
        const std::size_t m = cells.size();
        for (std::size_t k = 0; k < m; ++k) {
            local[static_cast<std::size_t>(cells[k])] = static_cast<std::int32_t>(k);
        }
        // Outlets: sea faces; else a pseudo outlet (decision D14).
        std::vector<double> sink(m, 0.0);
        std::size_t faces = 0;
        double face_h = 0.0;
        for (std::size_t k = 0; k < m; ++k) {
            const auto i = static_cast<std::size_t>(cells[k]);
            for (std::size_t d = 0; d < 4; ++d) {
                if (sea_face_bit(net.sea_face[i], static_cast<unsigned>(d))) {
                    sink[k] += net.K[i];
                    face_h += static_cast<double>(net.h[i]);
                    ++faces;
                    cell_flags[i] = static_cast<std::uint8_t>(cell_flags[i] | kOutlet);
                }
            }
        }
        const bool sea_connected = faces != 0;
        RiverInfo info;
        info.cells = m;
        info.sea_less = !sea_connected;
        info.x0 = info.y0 = kMax;
        info.x1 = info.y1 = kMin;
        for (const std::int32_t i : cells) {
            const auto u = static_cast<std::size_t>(i);
            info.x0 = std::min(info.x0, net.cx[u]), info.x1 = std::max(info.x1, net.cx[u]);
            info.y0 = std::min(info.y0, net.cy[u]), info.y1 = std::max(info.y1, net.cy[u]);
            if (!sea_connected) {
                cell_flags[u] = static_cast<std::uint8_t>(cell_flags[u] | RiverFlow::kSeaLess);
            }
        }
        if (faces == 0) {
            ++st.sea_less_components;
            // Candidates: the mouth cells if the river has any, else all.
            bool have_mouth = false;
            for (const std::int32_t i : cells) {
                have_mouth = have_mouth || (net.flags[static_cast<std::size_t>(i)] & kMouth) != 0;
            }
            auto lv = [&](std::size_t i) {
                return std::isnan(net.level[i]) ? 1e30F : net.level[i];
            };
            float min_level = 1e30F;
            for (const std::int32_t i : cells) {
                const auto u = static_cast<std::size_t>(i);
                if (!have_mouth || (net.flags[u] & kMouth) != 0) {
                    min_level = std::min(min_level, lv(u));
                }
            }
            std::int32_t pick = -1;
            for (const std::int32_t i : cells) {
                const auto u = static_cast<std::size_t>(i);
                if ((!have_mouth || (net.flags[u] & kMouth) != 0) && lv(u) <= min_level + 0.25F &&
                    (pick < 0 || net.bed[u] < net.bed[static_cast<std::size_t>(pick)])) {
                    pick = i;
                }
            }
            info.pseudo_cx = net.cx[static_cast<std::size_t>(pick)];
            info.pseudo_cy = net.cy[static_cast<std::size_t>(pick)];
            // The cell and its neighbours within two steps (a diamond: 5 cells
            // in a channel one cell wide, 13 in an open reach).
            std::vector<std::int32_t> ring = {pick};
            std::vector<std::int32_t> seen_cells = {pick};
            for (int step = 0; step < 2; ++step) {
                std::vector<std::int32_t> next;
                for (const std::int32_t i : ring) {
                    for (std::size_t d = 0; d < 4; ++d) {
                        const std::int32_t j = net.nb[4 * static_cast<std::size_t>(i) + d];
                        if (j >= 0 && std::find(seen_cells.begin(), seen_cells.end(), j) ==
                                          seen_cells.end()) {
                            seen_cells.push_back(j);
                            next.push_back(j);
                        }
                    }
                }
                ring = std::move(next);
            }
            for (const std::int32_t i : seen_cells) {
                const auto u = static_cast<std::size_t>(i);
                sink[static_cast<std::size_t>(local[u])] += net.K[u];
                face_h += static_cast<double>(net.h[u]);
                ++faces;
                cell_flags[u] = static_cast<std::uint8_t>(cell_flags[u] | kOutlet | kPseudo);
            }
        }

        // Eikonal distance to the outlets (cells); direction fallback, heads.
        std::vector<double> seed(m, 1e30), dist;
        for (std::size_t k = 0; k < m; ++k) {
            const auto i = static_cast<std::size_t>(cells[k]);
            if ((cell_flags[i] & kPseudo) != 0) {
                seed[k] = 0.0;
            } else if (net.sea_face[i] != 0) {
                seed[k] = 0.5;
            }
        }
        eikonal(net, cells, local, seed, dist);
        // Gradient of D at the cells (pointing away from the outlets); the
        // flow runs against it.
        for (std::size_t k = 0; k < m; ++k) {
            const auto i = static_cast<std::size_t>(cells[k]);
            double gxx = 0.0, gyy = 0.0;
            for (int axis = 0; axis < 2; ++axis) {
                const std::int32_t jm = net.nb[4 * i + static_cast<std::size_t>(axis) * 2];
                const std::int32_t jp = net.nb[4 * i + static_cast<std::size_t>(axis) * 2 + 1];
                auto dv = [&](std::int32_t j) {
                    return dist[static_cast<std::size_t>(local[static_cast<std::size_t>(j)])];
                };
                // A sea face continues the distance down to 0 at the sea.
                const bool sea_m = sea_face_bit(net.sea_face[i], static_cast<unsigned>(axis) * 2U);
                const bool sea_p =
                    sea_face_bit(net.sea_face[i], static_cast<unsigned>(axis) * 2U + 1U);
                const double here = dist[k];
                double a = here, b = here, span = 0.0;
                if (jm >= 0) {
                    a = dv(jm), span += 1.0;
                } else if (sea_m) {
                    a = 0.0, span += 1.0;
                }
                if (jp >= 0) {
                    b = dv(jp), span += 1.0;
                } else if (sea_p) {
                    b = 0.0, span += 1.0;
                }
                const double g = span > 0.0 ? (b - a) / span : 0.0;
                (axis == 0 ? gxx : gyy) = g;
            }
            dgx[i] = static_cast<float>(gxx);
            dgy[i] = static_cast<float>(gyy);
        }

        // Inflow weights per cell.
        std::vector<double> b(m, 1.0);
        double lambda_scale = 1.0; // physical flux per unit of solved flux
        bool sourced = false;
        if (cfg.discharge == RiverConfig::Discharge::Sourced) {
            std::vector<double> inflow(m, 0.0);
            double total = 0.0;
            for (std::size_t k = 0; k < m; ++k) {
                const auto i = static_cast<std::size_t>(cells[k]);
                if (dist[k] < static_cast<double>(cfg.source_min_tiles) * c) {
                    continue;
                }
                bool top = true;
                for (std::size_t d = 0; d < 4 && top; ++d) {
                    const std::int32_t j = net.nb[4 * i + d];
                    if (j < 0) {
                        continue;
                    }
                    const auto lj = static_cast<std::size_t>(local[static_cast<std::size_t>(j)]);
                    top = dist[k] > dist[lj] || (dist[k] == dist[lj] && k < lj);
                }
                if (!top) {
                    continue;
                }
                // A head: inflow v_src * (width x depth), spread over the
                // disc of half a width around the middle of the channel. The
                // local maximum of the distance sits in a corner of the dead
                // end; the middle is the cell with the widest clearance to the
                // banks within source_reach_tiles (measured along the river) of it.
                const double reach = static_cast<double>(cfg.source_reach_tiles) * c;
                std::size_t mid = i;
                for (std::size_t kk = 0; kk < m; ++kk) {
                    const auto ii = static_cast<std::size_t>(cells[kk]);
                    if (dist[kk] >= dist[k] - reach && net.dbank[ii] > net.dbank[mid]) {
                        mid = ii;
                    }
                }
                const double radius = std::max(1.0, static_cast<double>(net.dbank[mid]) - 0.5);
                const double width = 2.0 * radius * delta;
                const double q_src =
                    static_cast<double>(cfg.source_speed) * width * static_cast<double>(net.h[mid]);
                std::vector<std::size_t> disc;
                for (std::size_t kk = 0; kk < m; ++kk) {
                    const auto ii = static_cast<std::size_t>(cells[kk]);
                    const double dx = net.cx[ii] - net.cx[mid], dy = net.cy[ii] - net.cy[mid];
                    if (dx * dx + dy * dy <= radius * radius) {
                        disc.push_back(kk);
                    }
                }
                for (const std::size_t kk : disc) {
                    inflow[kk] += q_src / static_cast<double>(disc.size());
                }
                total += q_src;
            }
            if (total > 0.0) {
                b = inflow;
                sourced = true;
            }
        }

        // Operator.
        Operator a;
        a.n = m;
        a.nb.assign(4 * m, -1);
        a.g.assign(4 * m, 0.0);
        a.sink = sink;
        a.cx.resize(m);
        a.cy.resize(m);
        std::int32_t lx0 = kMax, ly0 = kMax;
        for (const std::int32_t i : cells) {
            lx0 = std::min(lx0, net.cx[static_cast<std::size_t>(i)]);
            ly0 = std::min(ly0, net.cy[static_cast<std::size_t>(i)]);
        }
        for (std::size_t k = 0; k < m; ++k) {
            const auto i = static_cast<std::size_t>(cells[k]);
            a.cx[k] = net.cx[i] - lx0;
            a.cy[k] = net.cy[i] - ly0;
            for (std::size_t d = 0; d < 4; ++d) {
                const std::int32_t j = net.nb[4 * i + d];
                if (j < 0) {
                    continue;
                }
                const auto ju = static_cast<std::size_t>(j);
                a.nb[4 * k + d] = local[ju];
                a.g[4 * k + d] = 2.0 * net.K[i] * net.K[ju] / (net.K[i] + net.K[ju]);
            }
        }
        a.finish();
        double b_total = 0.0;
        for (const double v : b) {
            b_total += v;
        }

        Solver solver(std::move(a), cfg);
        std::vector<double> phi;
        const std::uint32_t its = solver.solve(b, phi, cfg.cg_tolerance, cfg.cg_max_iterations);
        const double res = solver.residual(b, phi);
        bool finite = true;
        for (const double v : phi) {
            finite = finite && std::isfinite(v);
        }
        const bool ok = finite && res <= cfg.residual_limit;
        if (m > largest) {
            largest = m;
            st.levels = static_cast<std::uint32_t>(solver.levels());
        }
        st.iterations = std::max(st.iterations, std::min(its, cfg.cg_max_iterations));

        std::vector<std::array<double, 2>> q(m, {0.0, 0.0});
        if (ok) {
            st.residual = std::max(st.residual, res);
            // Face fluxes F (leaving the cell) -> cell-centre flux per unit width.
            double out_flux = 0.0;
            for (std::size_t k = 0; k < m; ++k) {
                const auto i = static_cast<std::size_t>(cells[k]);
                double f[4] = {0, 0, 0, 0};
                for (std::size_t d = 0; d < 4; ++d) {
                    const std::int32_t j = net.nb[4 * i + d];
                    if (j >= 0) {
                        const auto lj =
                            static_cast<std::size_t>(local[static_cast<std::size_t>(j)]);
                        f[d] = (phi[k] - phi[lj]) * 2.0 * net.K[i] *
                               net.K[static_cast<std::size_t>(j)] /
                               (net.K[i] + net.K[static_cast<std::size_t>(j)]);
                    } else if (sea_face_bit(net.sea_face[i], static_cast<unsigned>(d))) {
                        f[d] = phi[k] * net.K[i];
                        out_flux += f[d];
                    }
                }
                if ((cell_flags[i] & kPseudo) != 0) {
                    out_flux += phi[k] * net.K[i];
                }
                q[k] = {0.5 * (f[1] - f[0]), 0.5 * (f[3] - f[2])};
            }
            st.flux_imbalance =
                std::max(st.flux_imbalance, std::fabs(out_flux - b_total) / b_total);
            // Discharge model (rivers.md 2.4): physical flux per unit of solved flux.
            switch (cfg.discharge) {
            case RiverConfig::Discharge::Normalised: {
                if (cfg.reference == RiverConfig::Reference::MedianCell) {
                    // The median cell of the river, in the unit-flux model
                    // speed |q| / (delta h), gets median_speed.
                    std::vector<double> raw(m);
                    for (std::size_t k = 0; k < m; ++k) {
                        raw[k] = std::hypot(q[k][0], q[k][1]) /
                                 (delta *
                                  static_cast<double>(net.h[static_cast<std::size_t>(cells[k])]));
                    }
                    const auto mid = raw.begin() + static_cast<std::ptrdiff_t>(m / 2);
                    std::nth_element(raw.begin(), mid, raw.end());
                    if (*mid > 0.0) {
                        lambda_scale = static_cast<double>(cfg.median_speed) / *mid;
                        break;
                    }
                }
                // The reference section: bands one cell thick of the distance to
                // the outlets (D in [k, k + 1)); a band passes the inflow of
                // everything beyond it (N_up cells) through the area
                // delta * sum(h). The fastest band (at least out_min_cells wide)
                // that still passes out_flux_share of the river's discharge gets
                // out_speed. The mouths of the originals are broad sprite blobs;
                // measuring at their sea faces (rivers.md E7) would make the
                // river proper several times too fast.
                std::size_t max_band = 0;
                for (std::size_t k = 0; k < m; ++k) {
                    max_band = std::max(max_band, static_cast<std::size_t>(std::floor(dist[k])));
                }
                std::vector<double> band_h(max_band + 1, 0.0);
                std::vector<std::size_t> band_n(max_band + 1, 0);
                for (std::size_t k = 0; k < m; ++k) {
                    const auto band = static_cast<std::size_t>(std::floor(dist[k]));
                    band_h[band] += static_cast<double>(net.h[static_cast<std::size_t>(cells[k])]);
                    ++band_n[band];
                }
                double best = 0.0;
                std::size_t beyond = 0; // cells in bands beyond k
                for (std::size_t k = max_band + 1; k-- > 0;) {
                    const std::size_t passed = beyond + band_n[k];
                    if (band_n[k] >= cfg.out_min_cells && band_h[k] > 0.0 &&
                        static_cast<double>(passed) >=
                            static_cast<double>(cfg.out_flux_share) * static_cast<double>(m)) {
                        best = std::max(best, static_cast<double>(passed) / (delta * band_h[k]));
                    }
                    beyond = passed;
                }
                if (best <= 0.0) {
                    best = b_total / (delta * std::max(face_h, 1e-12));
                }
                lambda_scale = static_cast<double>(cfg.out_speed) / best;
                break;
            }
            case RiverConfig::Discharge::Absolute:
                lambda_scale = static_cast<double>(cfg.inflow_rate) * delta * delta;
                break;
            case RiverConfig::Discharge::Sourced:
                lambda_scale = sourced ? 1.0 : static_cast<double>(cfg.inflow_rate) * delta * delta;
                break;
            }
        } else {
            ++st.fallback_components;
            st.fell_back = true;
            if (cfg.log_fallback) {
                std::fprintf(stderr,
                             "river: %zu-cell river at cell (%d, %d) fell back to the Eikonal "
                             "field (residual %.3g after %u iterations)\n",
                             m, lx0, ly0, res, its);
            }
            for (std::size_t k = 0; k < m; ++k) {
                cell_flags[static_cast<std::size_t>(cells[k])] |= RiverFlow::kFallback;
            }
        }

        // Velocity: u = q / h, then the soft floor, falls, the cap.
        const float vf = cfg.speed_floor;
        solved_cells += m;
        for (std::size_t k = 0; k < m; ++k) {
            const auto i = static_cast<std::size_t>(cells[k]);
            const bool fallback = (cell_flags[i] & RiverFlow::kFallback) != 0;
            double vx = 0.0, vy = 0.0;
            if (!fallback) {
                const double s = lambda_scale / (delta * static_cast<double>(net.h[i]));
                vx = q[k][0] * s;
                vy = q[k][1] * s;
            }
            double mag = std::hypot(vx, vy);
            double dx = 0.0, dy = 0.0;
            if (fallback || mag < 1e-9) {
                // The Eikonal direction (downhill to the outlets).
                const double gm =
                    std::hypot(static_cast<double>(dgx[i]), static_cast<double>(dgy[i]));
                if (gm > 1e-9) {
                    dx = -static_cast<double>(dgx[i]) / gm;
                    dy = -static_cast<double>(dgy[i]) / gm;
                }
                mag = fallback ? static_cast<double>(cfg.out_speed) : 0.0;
            } else {
                dx = vx / mag;
                dy = vy / mag;
            }
            const double vf4 = static_cast<double>(vf) * vf * vf * vf;
            floor_cells += mag < static_cast<double>(vf) ? 1U : 0U;
            mag = std::pow(mag * mag * mag * mag + vf4, 0.25);
            const float rho = smoothstep(kRapidsFrom, kRapidsFull, net.slope[i]);
            mag = std::max(mag, static_cast<double>(rho * cfg.fall_speed));
            cap_cells += mag >= static_cast<double>(cfg.speed_cap) ? 1U : 0U;
            mag = std::min(mag, static_cast<double>(cfg.speed_cap));
            if (!sea_connected) {
                mag *= static_cast<double>(cfg.sea_less_speed_scale);
            }
            ux[i] = static_cast<float>(dx * mag);
            uy[i] = static_cast<float>(dy * mag);
        }

        out.rivers_.push_back(info);

        // Downstream consistency against the Eikonal direction.
        if (ok) {
            for (std::size_t k = 0; k < m; ++k) {
                const auto i = static_cast<std::size_t>(cells[k]);
                const double gm =
                    std::hypot(static_cast<double>(dgx[i]), static_cast<double>(dgy[i]));
                if (gm < 1e-9) {
                    continue;
                }
                const double dp =
                    -(static_cast<double>(ux[i]) * dgx[i] + static_cast<double>(uy[i]) * dgy[i]);
                ++down_total;
                down_good += dp > 0.0 ? 1U : 0U;
                if (sea_connected) {
                    ++down_total_sea;
                    down_good_sea += dp > 0.0 ? 1U : 0U;
                }
            }
        }
        for (std::size_t k = 0; k < m; ++k) {
            local[static_cast<std::size_t>(cells[k])] = -1;
        }
    }

    // --- ponds: a static eddy so lakes are not frozen (decision D7) ---
    for (const std::int32_t i : pond_cells_list) {
        const auto u = static_cast<std::size_t>(i);
        // Smooth value noise over the cells (a period of five cells, a fixed
        // seed plane): an eddy direction that varies slowly over a pond.
        const float th = 6.2831853F * noise3(0.2F * static_cast<float>(net.cx[u]),
                                             0.2F * static_cast<float>(net.cy[u]), 3.7F);
        ux[u] = cfg.pond_swirl * std::cos(th);
        uy[u] = cfg.pond_swirl * std::sin(th);
    }

    // --- one wetness-weighted 3 x 3 smoothing per pass, inside a river ---
    for (std::uint32_t pass = 0; pass < cfg.smoothing_passes; ++pass) {
        std::vector<float> sx = ux, sy = uy;
        for (std::size_t i = 0; i < net.n; ++i) {
            if (net.comp[i] < 0) {
                continue;
            }
            double wsum = 0.0, ax = 0.0, ay = 0.0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const std::int32_t j = id_at(net.cx[i] + dx, net.cy[i] + dy);
                    if (j < 0 || net.comp[static_cast<std::size_t>(j)] != net.comp[i]) {
                        continue;
                    }
                    const double w = (dx == 0 ? 2.0 : 1.0) * (dy == 0 ? 2.0 : 1.0);
                    wsum += w;
                    ax += w * ux[static_cast<std::size_t>(j)];
                    ay += w * uy[static_cast<std::size_t>(j)];
                }
            }
            sx[i] = static_cast<float>(ax / wsum);
            sy[i] = static_cast<float>(ay / wsum);
        }
        ux = std::move(sx);
        uy = std::move(sy);
    }

    // --- falls, mouths and bridges: the speed changes from the river's to the
    // fall's within less than a tile and the directions of a mouth fan point
    // at each other; a few extra passes, weighted by speed so that the slow
    // side does not drag the direction, over the cells within transition_reach
    // steps of them (review S4, rivers.md 14.6).
    if (cfg.transition_passes > 0 && cfg.transition_reach > 0) {
        constexpr std::uint8_t kTransition = kFall | kMouth | kBridge;
        std::vector<std::int32_t> dist(net.n, -1);
        std::vector<std::int32_t> frontier;
        for (std::size_t i = 0; i < net.n; ++i) {
            if (net.comp[i] >= 0 && (net.flags[i] & kTransition) != 0) {
                dist[i] = 0;
                frontier.push_back(static_cast<std::int32_t>(i));
            }
        }
        for (std::uint32_t step = 0; step < cfg.transition_reach && !frontier.empty(); ++step) {
            std::vector<std::int32_t> next;
            for (const std::int32_t i : frontier) {
                for (std::size_t d = 0; d < 4; ++d) {
                    const std::int32_t j = net.nb[4 * static_cast<std::size_t>(i) + d];
                    if (j >= 0 && dist[static_cast<std::size_t>(j)] < 0 &&
                        net.comp[static_cast<std::size_t>(j)] >= 0) {
                        dist[static_cast<std::size_t>(j)] = static_cast<std::int32_t>(step) + 1;
                        next.push_back(j);
                    }
                }
            }
            frontier = std::move(next);
        }
        const double vfloor = static_cast<double>(cfg.speed_floor) + 0.05;
        for (std::uint32_t pass = 0; pass < cfg.transition_passes; ++pass) {
            std::vector<float> sx = ux, sy = uy;
            for (std::size_t i = 0; i < net.n; ++i) {
                if (dist[i] < 0) {
                    continue;
                }
                double wsum = 0.0, ax = 0.0, ay = 0.0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        const std::int32_t j = id_at(net.cx[i] + dx, net.cy[i] + dy);
                        if (j < 0 || net.comp[static_cast<std::size_t>(j)] != net.comp[i]) {
                            continue;
                        }
                        const auto ju = static_cast<std::size_t>(j);
                        const double w =
                            (dx == 0 ? 2.0 : 1.0) * (dy == 0 ? 2.0 : 1.0) *
                            (std::hypot(static_cast<double>(ux[ju]), static_cast<double>(uy[ju])) +
                             vfloor);
                        wsum += w;
                        ax += w * ux[ju];
                        ay += w * uy[ju];
                    }
                }
                sx[i] = static_cast<float>(ax / wsum);
                sy[i] = static_cast<float>(ay / wsum);
            }
            ux = std::move(sx);
            uy = std::move(sy);
        }
    }

    // --- export ---
    out.cells_.resize(net.n);
    std::vector<float> speeds(net.n);
    double speed_sum = 0.0;
    for (std::size_t i = 0; i < net.n; ++i) {
        Cell& cell = out.cells_[i];
        cell.cx = net.cx[i];
        cell.cy = net.cy[i];
        cell.ux = ux[i];
        cell.uy = uy[i];
        cell.depth = net.h[i];
        cell.level = net.level[i];
        cell.river = net.comp[i] < 0 ? kPond : static_cast<std::uint32_t>(net.comp[i]);
        cell.slope = net.slope[i];
        cell.flags = cell_flags[i];
        if (!std::isfinite(cell.ux) || !std::isfinite(cell.uy)) {
            ++st.non_finite;
            cell.ux = cell.uy = 0.0F;
        }
        speeds[i] = std::hypot(cell.ux, cell.uy);
        speed_sum += speeds[i];
    }
    out.build_extension(cfg.extend_tiles);
    std::sort(speeds.begin(), speeds.end());
    st.speed_mean = static_cast<float>(speed_sum / static_cast<double>(net.n));
    st.speed_p50 = speeds[net.n / 2];
    st.speed_p95 = speeds[std::min(net.n - 1, net.n * 95 / 100)];
    st.speed_max = speeds.back();
    auto share = [](std::size_t good, std::size_t total) {
        return total > 0
                   ? static_cast<float>(static_cast<double>(good) / static_cast<double>(total))
                   : 0.0F;
    };
    st.downstream_fraction = share(down_good, down_total);
    st.downstream_fraction_sea = share(down_good_sea, down_total_sea);
    st.floor_fraction = share(floor_cells, solved_cells);
    st.cap_fraction = share(cap_cells, solved_cells);
    st.build_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t_start)
            .count();
    return out;
}

} // namespace anno::render::river
