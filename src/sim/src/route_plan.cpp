// plan_route: the two-level search of the original global ship route engine (see route.hpp).
//
// Evidence: docs/re/ship-navigation.md 9.2-9.4, RF-coarse sections 2, 6, 7 (coarse expansion, learn
// table, goal test), RD 5.1-5.3 (prepare, Run, fine stage), RJ section 1 (fine stage at block
// edges), RK 1.2-1.3 (density), RA3 (Offer / callback), all executed under emulation in the private
// notes. Nothing here is a heuristic: no A*, no smoothing, no line-of-sight shortcut.
//
// Two optimisations that cannot change the result: the coarse flood stops once every goal-flag cell
// has been popped (the original keeps flooding the window, but only goal cells make offers and the
// labels the trace reads never change after discovery), and the fine flood stops once the chosen
// goal tile has been labelled (labels never change after discovery either).
#include <algorithm>
#include <array>
#include <limits>

#include <anno/sim/route.hpp>

namespace anno::sim {

namespace {

struct Dir {
    std::int32_t dx;
    std::int32_t dz;
};

// Direction codes of the engine: 0 W, 1 SW, 2 S, 3 SE, 4 E, 5 NE, 6 N, 7 NW; z grows southward.
const std::array<Dir, 8> kDir = {
    {{-1, 0}, {-1, 1}, {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}}};

// Fine discovery order NW, NE, N, SW, SE, S, W, E (0x10014330).
const std::array<std::uint8_t, 8> kFineOrder = {7, 5, 6, 1, 3, 2, 0, 4};
const std::array<std::uint8_t, 8> kFineOrderReversed = {4, 0, 2, 3, 1, 6, 5, 7};

const std::uint8_t kFineWater = 8;
const std::uint8_t kFineLand = 9;
const std::uint8_t kFineStart = 0xa;

// The kernels (0x100cccf4 5 x 5, 0x100ccce0 3 x 3) and the density combine table (0x100c15b0).
const std::array<std::uint8_t, 25> kKernel5 = {0, 0, 1, 0, 0, 0, 1, 2, 1, 0, 1, 2, 3,
                                               2, 1, 0, 1, 2, 1, 0, 0, 0, 1, 0, 0};
const std::array<std::uint8_t, 9> kKernel3 = {1, 1, 1, 1, 2, 1, 1, 1, 1};
const std::array<std::uint8_t, 16> kDensityNext = {0, 1, 2, 3, 1, 1, 2, 3, 2, 2, 3, 3, 3, 3, 3, 3};

// Goal candidate order per arrival group (table 0x100c15d0): groups 0..3 sweep columns 7..0, rows
// 0..7, columns 0..7, rows 7..0, each line in the order 3,4,2,5,1,6,0,7; group 4 is the centre
// spiral.
struct Candidate {
    std::uint8_t col;
    std::uint8_t row;
};

using CandidateTables = std::array<std::array<Candidate, 64>, 5>;

CandidateTables make_candidates() {
    const std::array<std::uint8_t, 8> line = {3, 4, 2, 5, 1, 6, 0, 7};
    const std::array<std::array<std::uint8_t, 2>, 64> spiral = {{
        {3, 3}, {4, 3}, {3, 4}, {4, 4}, {2, 3}, {2, 4}, {5, 3}, {5, 4}, {3, 2}, {4, 2}, {3, 5},
        {4, 5}, {2, 2}, {5, 2}, {2, 5}, {5, 5}, {3, 1}, {4, 1}, {2, 1}, {5, 1}, {3, 6}, {4, 6},
        {2, 6}, {5, 6}, {1, 3}, {1, 4}, {1, 2}, {1, 5}, {6, 3}, {6, 4}, {6, 2}, {6, 5}, {1, 1},
        {6, 1}, {1, 6}, {6, 6}, {3, 0}, {4, 0}, {3, 7}, {4, 7}, {0, 3}, {0, 4}, {7, 3}, {7, 4},
        {2, 0}, {5, 0}, {2, 7}, {5, 7}, {0, 2}, {0, 5}, {7, 2}, {7, 5}, {1, 0}, {6, 0}, {1, 7},
        {6, 7}, {0, 1}, {0, 6}, {7, 1}, {7, 6}, {0, 0}, {7, 0}, {0, 7}, {7, 7},
    }};
    CandidateTables t{};
    for (std::uint32_t i = 0; i < 8; ++i) {
        for (std::uint32_t j = 0; j < 8; ++j) {
            t[0][i * 8 + j] = {static_cast<std::uint8_t>(7 - i), line[j]};
            t[1][i * 8 + j] = {line[j], static_cast<std::uint8_t>(i)};
            t[2][i * 8 + j] = {static_cast<std::uint8_t>(i), line[j]};
            t[3][i * 8 + j] = {line[j], static_cast<std::uint8_t>(7 - i)};
        }
    }
    for (std::uint32_t i = 0; i < 64; ++i) {
        t[4][i] = {spiral[i][0], spiral[i][1]};
    }
    return t;
}

// Edge of a cell that an exit in direction d (0 W, 2 S, 4 E, 6 N) crosses: 0 E, 1 N, 2 W, 3 S.
std::uint32_t edge_of_dir(std::uint32_t d) {
    switch (d) {
    case 0:
        return 2;
    case 2:
        return 3;
    case 4:
        return 0;
    default:
        return 1;
    }
}

// The mask bit "water entering at edge s leaves at edge t" (s != t): bit 3 s + k, k = (s + 3 - t)
// mod 4.
std::uint32_t pair_bit(std::uint32_t s, std::uint32_t t) {
    return 3u * s + ((s + 3u + 4u - t) & 3u);
}

// Entry masks of the neighbour in direction d (RF section 2): every connection that ends at the
// edge facing the current cell, plus the nibble flag of that edge.
std::uint32_t entry_mask(std::uint32_t d) {
    switch (d) {
    case 6:
        return 0x2111; // N: connections ending at S, nibble bit 13
    case 2:
        return 0x8444; // S: ending at N, bit 15
    case 0:
        return 0x4888; // W: ending at E, bit 14
    default:
        return 0x1222; // E: ending at W, bit 12
    }
}

struct Meter {
    std::uint64_t limit = 0;
    std::uint64_t high = 0;
    bool over = false;
    void note(std::uint64_t bytes) {
        high = std::max(high, bytes);
        if (bytes > limit) {
            over = true;
        }
    }
};

struct CoarseNode {
    std::uint32_t cost;
    std::uint32_t seq;
    std::int32_t cell;
};

struct FineNode {
    std::uint32_t cost;
    std::uint32_t seq;
    std::int32_t x;
    std::int32_t z;
};

struct Learned {
    std::int32_t cell;
    std::uint16_t mask;
};

class Planner {
  public:
    Planner(const CoastIndex& index, const RouteQuery& query, const RouteLimits& limits,
            const RouteCostTable& costs, const route_detail::Perturbation& pert)
        : idx_(index), q_(query), lim_(limits), costs_(costs), pert_(pert) {}

    RouteResult run();

  private:
    // ---- setup
    bool setup_window();
    void setup_goal();
    std::uint16_t start_nibble() const;

    // ---- one attempt
    bool coarse_after(const CoarseNode& a, const CoarseNode& b) const;
    bool fine_after(const FineNode& a, const FineNode& b) const;
    bool coarse_search(); // false: a budget failed (res_ set)
    void expand_coarse(std::int32_t cell, std::uint32_t label, std::uint32_t cost);
    void goal_test(std::int32_t cell, std::uint32_t label);
    std::uint32_t eff_word(std::int32_t cell) const;
    enum class FineOutcome : std::uint8_t { Budget, Success, Failed };
    FineOutcome fine_search();
    void add_block(std::int32_t cell);
    bool ring_visited(std::int32_t cell) const;
    void finish_trace();
    void set_work();
    std::uint8_t* fine_tile(std::int32_t tx, std::int32_t tz);
    bool fine_enterable(std::int32_t tx, std::int32_t tz);
    void note_memory();
    std::uint64_t bytes_now() const;
    /// Accounts `extra` bytes about to be allocated; false (and the limit flag set) when the limit
    /// would be passed.
    bool grow(std::uint64_t extra);
    bool fail_limit(RouteLimit which);

    std::int32_t cell_x(std::int32_t cell) const { return cell % ncx_; }
    std::int32_t cell_z(std::int32_t cell) const { return cell / ncx_; }
    bool tile_water(std::int32_t tx, std::int32_t tz) const {
        return idx_.source().tile_is_water(tx, tz);
    }
    bool in_disc(std::int64_t tx, std::int64_t tz) const;

    const CoastIndex& idx_;
    const RouteQuery& q_;
    const RouteLimits& lim_;
    const RouteCostTable& costs_;
    const route_detail::Perturbation& pert_;
    RouteResult res_;
    Meter meter_;

    // window (tile origin ox_, oz_ is 8-aligned; cells are local)
    std::int64_t ox_ = 0, oz_ = 0;
    std::int32_t ncx_ = 0, ncz_ = 0;
    std::int32_t wcx0_ = 0, wcz0_ = 0; // world cell of local cell (0, 0)
    std::int32_t start_cell_ = 0;
    std::int32_t start_tx_ = 0, start_tz_ = 0;
    std::uint16_t start_nib_ = 0;
    std::int32_t goal_center_ = -1; // local cell of the disc centre, -1 when outside the window
    std::int64_t goal_tx_ = 0, goal_tz_ = 0; // goal TILE (ftol)
    std::vector<std::uint8_t> disc_tile_;    // half-widths of the tile disc
    std::vector<std::uint8_t> cell_flags_;   // bit 0: goal-flag cell
    std::int32_t goal_cells_ = 0;
    std::int64_t score_limit_ = 0;
    const RouteClassCosts* cc_ = nullptr;
    CandidateTables cand_{};

    // per attempt
    std::vector<std::uint8_t> labels_;
    std::vector<CoarseNode> heap_;
    std::vector<Learned> learned_;
    std::uint32_t seq_ = 0;
    // goal selection
    bool has_ = false;
    std::int64_t best_ = 0;
    std::int32_t stored_cell_ = 0;
    std::int32_t stored_tx_ = 0, stored_tz_ = 0;
    std::uint32_t stored_density_ = 0;
    std::int32_t stored_score_ = 0;
    bool exact_hit_ = false;
    std::span<const RouteStamp> stamps_;
    // corridor + fine
    std::vector<std::int32_t> path_;     // goal-first coarse path
    std::vector<std::int32_t> ordinal_;  // local cell -> block ordinal or -1
    std::vector<std::int32_t> corridor_; // local cells that have a block
    std::vector<std::uint8_t> blocks_;   // 64 bytes per ordinal
    std::vector<FineNode> fheap_;
    std::uint32_t fseq_ = 0;
    std::vector<std::uint8_t> trace_dirs_;
};

bool Planner::fail_limit(RouteLimit which) {
    res_.status = RouteStatus::BudgetExceeded;
    res_.exceeded = which;
    res_.runs.clear();
    res_.end_x = res_.end_z = res_.score = res_.best_score = 0;
    return false;
}

std::uint64_t Planner::bytes_now() const {
    const std::uint64_t cells = static_cast<std::uint64_t>(ncx_) * static_cast<std::uint64_t>(ncz_);
    std::uint64_t b = cells * (sizeof(std::uint8_t) * 2 + sizeof(std::int32_t));
    b += heap_.size() * sizeof(CoarseNode);
    b += fheap_.size() * sizeof(FineNode);
    b += blocks_.size();
    b += corridor_.size() * sizeof(std::int32_t);
    b += path_.size() * sizeof(std::int32_t);
    b += learned_.size() * sizeof(Learned);
    b += trace_dirs_.size();
    b += res_.runs.size() * sizeof(RouteRun);
    return b;
}

void Planner::note_memory() {
    meter_.note(bytes_now());
}

bool Planner::grow(std::uint64_t extra) {
    meter_.note(bytes_now() + extra);
    return !meter_.over;
}

bool Planner::in_disc(std::int64_t tx, std::int64_t tz) const {
    const std::int64_t dz = tz > goal_tz_ ? tz - goal_tz_ : goal_tz_ - tz;
    if (dz >= static_cast<std::int64_t>(disc_tile_.size())) {
        return false;
    }
    const std::int64_t dx = tx > goal_tx_ ? tx - goal_tx_ : goal_tx_ - tx;
    return dx <= static_cast<std::int64_t>(disc_tile_[static_cast<std::uint64_t>(dz)]);
}

bool Planner::setup_window() {
    const std::int64_t w = idx_.width_tiles();
    const std::int64_t h = idx_.height_tiles();
    const std::int64_t r = lim_.window_radius_tiles;
    // SetWindow 0x1000edd0: box centred on the start tile, clipped to the world, origin floored
    // to 8.
    const std::int64_t x0 = std::max<std::int64_t>(0, q_.start_x - r);
    const std::int64_t z0 = std::max<std::int64_t>(0, q_.start_z - r);
    const std::int64_t x1 = std::min<std::int64_t>(w, q_.start_x + r + 1);
    const std::int64_t z1 = std::min<std::int64_t>(h, q_.start_z + r + 1);
    ox_ = x0 - (x0 & 7);
    oz_ = z0 - (z0 & 7);
    const std::int64_t cx = (x1 - ox_ + 7) / 8;
    const std::int64_t cz = (z1 - oz_ + 7) / 8;
    const std::int64_t bx = (cx + 7) / 8;
    const std::int64_t bz = (cz + 7) / 8;
    if (bx > static_cast<std::int64_t>(lim_.max_window_blocks) ||
        bz > static_cast<std::int64_t>(lim_.max_window_blocks)) {
        return fail_limit(RouteLimit::Window);
    }
    const std::uint64_t cells = static_cast<std::uint64_t>(cx) * static_cast<std::uint64_t>(cz);
    meter_.note(cells * (sizeof(std::uint8_t) * 2 + sizeof(std::int32_t)));
    if (meter_.over) {
        return fail_limit(RouteLimit::Memory);
    }
    ncx_ = static_cast<std::int32_t>(cx);
    ncz_ = static_cast<std::int32_t>(cz);
    wcx0_ = static_cast<std::int32_t>(ox_ / 8);
    wcz0_ = static_cast<std::int32_t>(oz_ / 8);
    labels_.assign(cells, 0);
    cell_flags_.assign(cells, 0);
    ordinal_.assign(cells, -1);
    return true;
}

void Planner::setup_goal() {
    // ftol truncates toward zero; the Q24.8 value divides exactly.
    goal_tx_ = static_cast<std::int64_t>(q_.goal_x_q8) / 256;
    goal_tz_ = static_cast<std::int64_t>(q_.goal_z_q8) / 256;
    score_limit_ = 64ll * lim_.goal_radius_tiles * lim_.goal_radius_tiles;
    disc_tile_ = disc_half_widths(lim_.goal_radius_tiles);
    // Cell of a point: ftol((x - x0) * 0.125), i.e. truncation of (x_q8 - 256 x0) / 2048.
    const std::int64_t gcx = (static_cast<std::int64_t>(q_.goal_x_q8) - 256 * ox_) / 2048;
    const std::int64_t gcz = (static_cast<std::int64_t>(q_.goal_z_q8) - 256 * oz_) / 2048;
    const std::int32_t rc = (lim_.goal_radius_tiles + 4) / 8; // ftol((r + 4.0) * 0.125)
    const std::vector<std::uint8_t> hw = disc_half_widths(rc);
    goal_cells_ = 0;
    for (std::int32_t dz = -rc; dz <= rc; ++dz) {
        const std::int32_t half = hw[static_cast<std::uint64_t>(dz < 0 ? -dz : dz)];
        for (std::int32_t dx = -half; dx <= half; ++dx) {
            const std::int64_t lx = gcx + dx;
            const std::int64_t lz = gcz + dz;
            if (lx < 0 || lz < 0 || lx >= ncx_ || lz >= ncz_) {
                continue;
            }
            std::uint8_t& f = cell_flags_[static_cast<std::uint64_t>(lz * ncx_ + lx)];
            if ((f & 1u) == 0) {
                f |= 1u;
                ++goal_cells_;
            }
        }
    }
    if (gcx >= 0 && gcz >= 0 && gcx < ncx_ && gcz < ncz_) {
        goal_center_ = static_cast<std::int32_t>(gcz * ncx_ + gcx);
    }
}

std::uint16_t Planner::start_nibble() const {
    const std::int32_t cx0 = static_cast<std::int32_t>(ox_) + 8 * (start_cell_ % ncx_);
    const std::int32_t cz0 = static_cast<std::int32_t>(oz_) + 8 * (start_cell_ / ncx_);
    std::array<std::uint8_t, 64> water{};
    for (std::int32_t dz = 0; dz < 8; ++dz) {
        for (std::int32_t dx = 0; dx < 8; ++dx) {
            water[static_cast<std::uint64_t>(dx + 8 * dz)] = tile_water(cx0 + dx, cz0 + dz) ? 1 : 0;
        }
    }
    return route_detail::start_nibble(std::span<const std::uint8_t, 64>(water), start_tx_ - cx0,
                                      start_tz_ - cz0);
}

std::uint32_t Planner::eff_word(std::int32_t cell) const {
    std::uint32_t w = idx_.mask(wcx0_ + cell_x(cell), wcz0_ + cell_z(cell));
    if (cell == start_cell_) {
        w |= start_nib_;
    }
    if (cell == goal_center_) {
        w |= 0xf000u;
    }
    for (const Learned& l : learned_) {
        if (l.cell == cell) {
            w &= l.mask;
        }
    }
    return w;
}

void Planner::expand_coarse(std::int32_t cell, std::uint32_t label, std::uint32_t cost) {
    const std::int32_t cx = cell_x(cell);
    const std::int32_t cz = cell_z(cell);
    const std::uint32_t wc = eff_word(cell);
    // Discovery order and exit test per arrival label (RF section 2).
    std::array<std::uint32_t, 4> order{};
    std::uint32_t n = 0;
    std::uint32_t entry_edge = 0;
    const bool labelled = label >= 0x80 && label <= 0x86 && (label & 1u) == 0;
    if (labelled) {
        const std::uint32_t a = label - 0x80;
        entry_edge = a / 2;
        std::uint32_t p0 = (a == 0 || a == 4) ? 6u : 0u;
        std::uint32_t p1 = (a == 0 || a == 4) ? 2u : 4u;
        if (pert_.coarse_swap_perp) {
            std::swap(p0, p1);
        }
        order = pert_.coarse_straight_first ? std::array<std::uint32_t, 4>{a, p0, p1, 0}
                                            : std::array<std::uint32_t, 4>{p0, p1, a, 0};
        n = 3;
    } else {
        order = pert_.coarse_start_order_alt ? std::array<std::uint32_t, 4>{0, 4, 6, 2}
                                             : std::array<std::uint32_t, 4>{6, 2, 0, 4};
        n = 4;
    }
    for (std::uint32_t i = 0; i < n; ++i) {
        const std::uint32_t d = order[i];
        const std::uint32_t bit = labelled ? pair_bit(entry_edge, edge_of_dir(d)) : (12u + d / 2u);
        if ((wc & (1u << bit)) == 0) {
            continue; // exit test on the current cell
        }
        const std::int32_t nx = cx + kDir[d].dx;
        const std::int32_t nz = cz + kDir[d].dz;
        if (nx < 0 || nz < 0 || nx >= ncx_ || nz >= ncz_) {
            continue; // outside the window
        }
        const std::int32_t nb = nz * ncx_ + nx;
        if (labels_[static_cast<std::uint64_t>(nb)] != 0) {
            continue; // labelled cells have class mask 0: entered once
        }
        if ((eff_word(nb) & entry_mask(d)) == 0) {
            continue; // entry test on the neighbour
        }
        const std::uint16_t type = idx_.type(wcx0_ + nx, wcz0_ + nz);
        std::uint32_t step = 0;
        const bool straight = !labelled || d == label - 0x80;
        if (type == 3) {
            if (!cc_->shore_enterable) {
                continue;
            }
            step = straight ? cc_->shore_orth
                            : static_cast<std::uint32_t>(cc_->shore_diag - cc_->shore_orth);
        } else {
            step = straight ? cc_->open_orth
                            : static_cast<std::uint32_t>(cc_->open_diag - cc_->open_orth);
        }
        if (!grow(sizeof(CoarseNode))) {
            return; // the memory limit would be passed by this node; the search loop reports it
        }
        labels_[static_cast<std::uint64_t>(nb)] = static_cast<std::uint8_t>(0x80 + d);
        heap_.push_back({cost + step, seq_++, nb});
        const bool lifo = pert_.lifo_coarse_ties;
        std::push_heap(heap_.begin(), heap_.end(),
                       [lifo](const CoarseNode& a, const CoarseNode& b) {
                           if (a.cost != b.cost) {
                               return a.cost > b.cost;
                           }
                           return lifo ? a.seq < b.seq : a.seq > b.seq;
                       });
    }
}

void Planner::goal_test(std::int32_t cell, std::uint32_t label) {
    const std::uint32_t group =
        (label >= 0x80 && label <= 0x87) ? ((label + 0x80u) & 0xffu) >> 1 : 4u;
    const std::int64_t cx0 = ox_ + 8ll * cell_x(cell);
    const std::int64_t cz0 = oz_ + 8ll * cell_z(cell);
    for (const Candidate& c : cand_[group]) {
        const std::int64_t tx = cx0 + c.col;
        const std::int64_t tz = cz0 + c.row;
        if (!in_disc(tx, tz) ||
            !tile_water(static_cast<std::int32_t>(tx), static_cast<std::int32_t>(tz))) {
            continue;
        }
        // 2304 - ftol(64 d^2), d from the tile centre (+0.5) to the Q24.8 goal, in integers.
        const std::int64_t dxq = (2 * tx + 1) * 128 - q_.goal_x_q8;
        const std::int64_t dzq = (2 * tz + 1) * 128 - q_.goal_z_q8;
        const std::int64_t t = (dxq * dxq + dzq * dzq + (pert_.round_score ? 512 : 0)) / 1024;
        const std::int64_t score = score_limit_ - t;
        if (score <= 0 || score <= best_) {
            continue; // the callback offers only a strictly better score, and never d >= radius
        }
        const std::uint32_t dens =
            route_density(stamps_, static_cast<std::int32_t>(tx), static_cast<std::int32_t>(tz));
        if (dens == 0) {
            // Offer returns 1: store, raise best.
            has_ = true;
            stored_cell_ = cell;
            stored_tx_ = static_cast<std::int32_t>(tx);
            stored_tz_ = static_cast<std::int32_t>(tz);
            stored_density_ = 0;
            stored_score_ = static_cast<std::int32_t>(score);
            best_ = score;
            if (t <= 0) {
                exact_hit_ = true;
                return;
            }
        } else if (has_ && dens >= stored_density_) {
            // rejected: not strictly less congested than the stored node
        } else {
            // stored, best score NOT raised (the original's quirk)
            has_ = true;
            stored_cell_ = cell;
            stored_tx_ = static_cast<std::int32_t>(tx);
            stored_tz_ = static_cast<std::int32_t>(tz);
            stored_density_ = dens;
            stored_score_ = static_cast<std::int32_t>(score);
        }
    }
}

bool Planner::coarse_after(const CoarseNode& a, const CoarseNode& b) const {
    if (a.cost != b.cost) {
        return a.cost > b.cost;
    }
    return pert_.lifo_coarse_ties ? a.seq < b.seq : a.seq > b.seq;
}

bool Planner::fine_after(const FineNode& a, const FineNode& b) const {
    if (a.cost != b.cost) {
        return a.cost > b.cost;
    }
    return pert_.lifo_fine_ties ? a.seq < b.seq : a.seq > b.seq;
}

bool Planner::coarse_search() {
    std::fill(labels_.begin(), labels_.end(), std::uint8_t{0});
    heap_.clear();
    seq_ = 0;
    has_ = false;
    best_ = 0;
    exact_hit_ = false;
    stored_density_ = 0;
    std::int32_t remaining = goal_cells_;
    labels_[static_cast<std::uint64_t>(start_cell_)] = 7;
    heap_.push_back({0, seq_++, start_cell_});
    const auto after = [this](const CoarseNode& a, const CoarseNode& b) {
        return coarse_after(a, b);
    };
    while (!heap_.empty()) {
        note_memory();
        if (meter_.over) {
            return fail_limit(RouteLimit::Memory);
        }
        if (res_.work.coarse_pops >= lim_.coarse_budget) {
            return fail_limit(RouteLimit::CoarsePops);
        }
        std::pop_heap(heap_.begin(), heap_.end(), after);
        const CoarseNode node = heap_.back();
        heap_.pop_back();
        ++res_.work.coarse_pops;
        const std::uint32_t label = labels_[static_cast<std::uint64_t>(node.cell)];
        if ((cell_flags_[static_cast<std::uint64_t>(node.cell)] & 1u) != 0) {
            goal_test(node.cell, label);
            --remaining;
            if (exact_hit_ || remaining == 0) {
                break; // an exact hit stops the search; with every goal cell popped nothing can
                       // change
            }
        }
        expand_coarse(node.cell, label, node.cost);
    }
    note_memory();
    return !meter_.over || fail_limit(RouteLimit::Memory);
}

void Planner::add_block(std::int32_t cell) {
    if (ordinal_[static_cast<std::uint64_t>(cell)] >= 0) {
        return;
    }
    if (!grow(64 + sizeof(std::int32_t))) {
        return; // one fine block (64 bytes) and its ordinal entry would pass the memory limit
    }
    ordinal_[static_cast<std::uint64_t>(cell)] = static_cast<std::int32_t>(corridor_.size());
    corridor_.push_back(cell);
    const std::int32_t cx0 = static_cast<std::int32_t>(ox_) + 8 * cell_x(cell);
    const std::int32_t cz0 = static_cast<std::int32_t>(oz_) + 8 * cell_z(cell);
    for (std::int32_t dz = 0; dz < 8; ++dz) {
        for (std::int32_t dx = 0; dx < 8; ++dx) {
            blocks_.push_back(tile_water(cx0 + dx, cz0 + dz) ? kFineWater : kFineLand);
        }
    }
}

std::uint8_t* Planner::fine_tile(std::int32_t tx, std::int32_t tz) {
    const std::int64_t lx = static_cast<std::int64_t>(tx) - ox_;
    const std::int64_t lz = static_cast<std::int64_t>(tz) - oz_;
    if (lx < 0 || lz < 0 || lx >= 8ll * ncx_ || lz >= 8ll * ncz_) {
        return nullptr;
    }
    const std::int32_t o = ordinal_[static_cast<std::uint64_t>((lz / 8) * ncx_ + (lx / 8))];
    if (o < 0) {
        return nullptr;
    }
    return &blocks_[static_cast<std::uint64_t>(o) * 64u +
                    static_cast<std::uint64_t>((lz % 8) * 8 + (lx % 8))];
}

bool Planner::fine_enterable(std::int32_t tx, std::int32_t tz) {
    const std::uint8_t* t = fine_tile(tx, tz);
    return t != nullptr && *t == kFineWater;
}

bool Planner::ring_visited(std::int32_t cell) const {
    const std::int32_t o = ordinal_[static_cast<std::uint64_t>(cell)];
    if (o < 0) {
        return false;
    }
    const std::uint8_t* b = &blocks_[static_cast<std::uint64_t>(o) * 64u];
    for (std::uint32_t i = 0; i < 8; ++i) {
        const std::uint8_t ring[4] = {b[i], b[56 + i], b[8 * i], b[8 * i + 7]};
        for (const std::uint8_t v : ring) {
            if (v < 8 || v == kFineStart) {
                return true;
            }
        }
    }
    return false;
}

Planner::FineOutcome Planner::fine_search() {
    // The coarse path, goal cell first, read back from the labels.
    path_.clear();
    {
        std::int32_t cell = stored_cell_;
        const std::uint64_t cap =
            static_cast<std::uint64_t>(ncx_) * static_cast<std::uint64_t>(ncz_);
        for (std::uint64_t guard = 0; guard <= cap; ++guard) {
            if (!grow(sizeof(std::int32_t))) {
                break;
            }
            path_.push_back(cell);
            const std::uint32_t label = labels_[static_cast<std::uint64_t>(cell)];
            if (label < 0x80 || label > 0x86) {
                break; // the start cell (label 7)
            }
            const Dir& dd = kDir[label - 0x80];
            cell = (cell_z(cell) - dd.dz) * ncx_ + (cell_x(cell) - dd.dx);
        }
    }
    if (meter_.over) {
        fail_limit(RouteLimit::Memory);
        return FineOutcome::Budget;
    }
    // The corridor: the path's blocks plus the missing corner of every 2 x 2 turn.
    for (const std::int32_t c : corridor_) {
        ordinal_[static_cast<std::uint64_t>(c)] = -1;
    }
    corridor_.clear();
    blocks_.clear();
    for (const std::int32_t c : path_) {
        add_block(c);
    }
    for (std::size_t i = 2; i < path_.size(); ++i) {
        const std::int32_t ax = cell_x(path_[i - 2]), az = cell_z(path_[i - 2]);
        const std::int32_t bx = cell_x(path_[i - 1]), bz = cell_z(path_[i - 1]);
        const std::int32_t cx = cell_x(path_[i]), cz = cell_z(path_[i]);
        if (ax != cx && az != cz) {
            const bool mid_is_cx_az = bx == cx && bz == az;
            add_block(mid_is_cx_az ? cz * ncx_ + ax : az * ncx_ + cx);
        }
    }
    note_memory();
    if (meter_.over) {
        fail_limit(RouteLimit::Memory);
        return FineOutcome::Budget;
    }
    fheap_.clear();
    fseq_ = 0;
    *fine_tile(start_tx_, start_tz_) = kFineStart;
    fheap_.push_back({0, fseq_++, start_tx_, start_tz_});
    const auto after = [this](const FineNode& a, const FineNode& b) { return fine_after(a, b); };
    std::array<std::uint8_t, 8> order = pert_.reverse_fine_order ? kFineOrderReversed : kFineOrder;
    const auto swap_codes = [&order](std::uint8_t a, std::uint8_t b) {
        std::iter_swap(std::find(order.begin(), order.end(), a),
                       std::find(order.begin(), order.end(), b));
    };
    if (pert_.swap_fine_nw_ne) {
        swap_codes(7, 5);
    }
    if (pert_.swap_fine_sw_se) {
        swap_codes(1, 3);
    }
    if (pert_.swap_fine_w_e) {
        swap_codes(0, 4);
    }
    bool found = stored_tx_ == start_tx_ && stored_tz_ == start_tz_;
    while (!found && !fheap_.empty()) {
        note_memory();
        if (meter_.over) {
            fail_limit(RouteLimit::Memory);
            return FineOutcome::Budget;
        }
        if (res_.work.fine_pops >= lim_.fine_budget) {
            fail_limit(RouteLimit::FinePops);
            return FineOutcome::Budget;
        }
        std::pop_heap(fheap_.begin(), fheap_.end(), after);
        const FineNode node = fheap_.back();
        fheap_.pop_back();
        ++res_.work.fine_pops;
        for (const std::uint8_t d : order) {
            const Dir& dd = kDir[d];
            const std::int32_t nx = node.x + dd.dx;
            const std::int32_t nz = node.z + dd.dz;
            const bool diagonal = dd.dx != 0 && dd.dz != 0;
            bool ok = fine_enterable(nx, nz);
            if (ok && diagonal && !pert_.allow_corner_cut) {
                ok = fine_enterable(node.x + dd.dx, node.z) &&
                     fine_enterable(node.x, node.z + dd.dz);
            }
            if (!ok) {
                continue;
            }
            if (!grow(sizeof(FineNode))) {
                break; // the loop head reports the memory limit
            }
            *fine_tile(nx, nz) = d;
            fheap_.push_back(
                {node.cost + (diagonal ? pert_.fine_diag_cost : 10u), fseq_++, nx, nz});
            std::push_heap(fheap_.begin(), fheap_.end(), after);
            if (nx == stored_tx_ && nz == stored_tz_) {
                found = true;
            }
        }
    }
    const std::uint8_t* g = fine_tile(stored_tx_, stored_tz_);
    const bool reached = g != nullptr && (*g < 8 || *g == kFineStart);
    return reached ? FineOutcome::Success : FineOutcome::Failed;
}

void Planner::finish_trace() {
    // Trace 0x10011640: follow the labels from the goal tile back to the start mark, then emit runs
    // of equal directions (at most run_cap) from the start.
    trace_dirs_.clear();
    std::int32_t x = stored_tx_;
    std::int32_t z = stored_tz_;
    const std::uint64_t guard = blocks_.size() + 1;
    for (std::uint64_t i = 0; i < guard; ++i) {
        const std::uint8_t b = *fine_tile(x, z);
        if (b == kFineStart) {
            break;
        }
        if (!grow(1)) {
            fail_limit(RouteLimit::Memory);
            return;
        }
        trace_dirs_.push_back(b);
        x -= kDir[b].dx;
        z -= kDir[b].dz;
    }
    res_.runs.clear();
    for (auto it = trace_dirs_.rbegin(); it != trace_dirs_.rend(); ++it) {
        if (!res_.runs.empty() && res_.runs.back().dir == *it &&
            res_.runs.back().steps < lim_.run_cap) {
            ++res_.runs.back().steps;
        } else {
            if (!grow(sizeof(RouteRun))) {
                fail_limit(RouteLimit::Memory);
                return;
            }
            res_.runs.push_back({*it, 1});
        }
    }
    res_.end_x = stored_tx_;
    res_.end_z = stored_tz_;
    res_.score = stored_score_;
    res_.best_score = static_cast<std::int32_t>(best_);
    res_.status = res_.runs.empty() ? RouteStatus::Arrived : RouteStatus::Planned;
    note_memory();
    if (meter_.over) {
        fail_limit(RouteLimit::Memory);
    }
}

void Planner::set_work() {
    res_.work.learned = static_cast<std::uint32_t>(learned_.size());
    res_.work.bytes_high = meter_.high;
}

RouteResult Planner::run() {
    if (!validate(lim_).empty() || !validate(costs_).empty() ||
        q_.nav_class >= costs_.classes.size() || q_.start_x < 0 || q_.start_z < 0 ||
        q_.start_x >= idx_.width_tiles() || q_.start_z >= idx_.height_tiles() || !idx_.valid()) {
        res_.status = RouteStatus::InvalidQuery;
        return res_;
    }
    cc_ = &costs_.classes[q_.nav_class];
    meter_.limit = lim_.memory_bytes;
    stamps_ = std::span<const RouteStamp>(q_.stamps).first(
        std::min<std::size_t>(q_.stamps.size(), lim_.unit_cap));
    cand_ = make_candidates();
    if (!setup_window()) {
        set_work();
        return res_;
    }
    setup_goal();
    if (goal_cells_ == 0) {
        res_.status = RouteStatus::OutsideWindow;
        set_work();
        return res_;
    }
    start_tx_ = q_.start_x;
    start_tz_ = q_.start_z;
    start_cell_ = static_cast<std::int32_t>(((start_tz_ - oz_) / 8) * ncx_ + (start_tx_ - ox_) / 8);
    start_nib_ = start_nibble();
    res_.status = RouteStatus::Unreachable;
    for (;;) {
        if (!coarse_search()) {
            break;
        }
        if (!has_) {
            res_.status = RouteStatus::Unreachable;
            break;
        }
        const FineOutcome fo = fine_search();
        if (fo == FineOutcome::Budget) {
            break;
        }
        if (fo == FineOutcome::Success) {
            finish_trace();
            break;
        }
        // The fine flood could not reach the goal tile: clear one connectivity bit of a coarse cell
        // on the path (0x10013fb0 and the blame selection of Refine) and search again.
        const std::size_t n = path_.size();
        if (n < 2) {
            res_.status = RouteStatus::Unreachable;
            break;
        }
        std::size_t f = n - 1;
        for (std::size_t j = 0; j + 1 < n; ++j) { // start side first
            if (!ring_visited(path_[n - 1 - j])) {
                f = j;
                break;
            }
        }
        if (f == 0) {
            res_.status = RouteStatus::Unreachable;
            break;
        }
        if (learned_.size() >= lim_.retry_cap) {
            res_.status = RouteStatus::Unreachable;
            res_.work.retry_cap_hit = true;
            break;
        }
        const std::int32_t p_cell = path_[n - f];
        const std::int32_t f_cell = path_[n - 1 - f];
        const std::uint16_t mask =
            route_detail::learn_mask(labels_[static_cast<std::uint64_t>(p_cell)],
                                     labels_[static_cast<std::uint64_t>(f_cell)]);
        if (mask == 0xffff) {
            res_.status = RouteStatus::Unreachable;
            break;
        }
        learned_.push_back({p_cell, mask});
    }
    set_work();
    return res_;
}

} // namespace

std::uint32_t route_density(std::span<const RouteStamp> stamps, std::int32_t tile_x,
                            std::int32_t tile_z) {
    std::uint32_t state = 0;
    for (const RouteStamp& s : stamps) {
        const std::int64_t radius = s.kernel == StampKernel::K5 ? 2 : 1;
        const std::int64_t side = 2 * radius + 1;
        const std::int64_t col =
            static_cast<std::int64_t>(tile_x) - (static_cast<std::int64_t>(s.tile_x) - radius);
        const std::int64_t row =
            static_cast<std::int64_t>(tile_z) - (static_cast<std::int64_t>(s.tile_z) - radius);
        if (col < 0 || row < 0 || col >= side || row >= side) {
            continue;
        }
        const std::uint32_t k = s.kernel == StampKernel::K5
                                    ? kKernel5[static_cast<std::uint64_t>(row * side + col)]
                                    : kKernel3[static_cast<std::uint64_t>(row * side + col)];
        state = kDensityNext[state * 4u + k];
    }
    return state;
}

namespace route_detail {

std::uint16_t start_nibble(std::span<const std::uint8_t, 64> water, std::int32_t lx,
                           std::int32_t lz) {
    if (lx < 0 || lz < 0 || lx > 7 || lz > 7) {
        return 0;
    }
    std::array<bool, 64> seen{};
    std::array<std::uint8_t, 64> stack{};
    std::uint32_t top = 0;
    const std::uint32_t s = static_cast<std::uint32_t>(lx + 8 * lz);
    seen[s] = true;
    stack[top++] = static_cast<std::uint8_t>(s);
    std::uint32_t nib = 0;
    while (top > 0) {
        const std::uint32_t i = stack[--top];
        const std::uint32_t x = i & 7u;
        const std::uint32_t z = i >> 3;
        if (x == 0) {
            nib |= 0x1000;
        }
        if (z == 7) {
            nib |= 0x2000;
        }
        if (x == 7) {
            nib |= 0x4000;
        }
        if (z == 0) {
            nib |= 0x8000;
        }
        const auto visit = [&](std::uint32_t nx, std::uint32_t nz) {
            const std::uint32_t j = nx + 8u * nz;
            if (!seen[j] && water[j] != 0) {
                seen[j] = true;
                stack[top++] = static_cast<std::uint8_t>(j);
            }
        };
        if (x > 0) {
            visit(x - 1, z);
        }
        if (x < 7) {
            visit(x + 1, z);
        }
        if (z > 0) {
            visit(x, z - 1);
        }
        if (z < 7) {
            visit(x, z + 1);
        }
    }
    return static_cast<std::uint16_t>(nib);
}

std::uint16_t learn_mask(std::uint32_t p_label, std::uint32_t f_label) {
    if (f_label < 0x80 || f_label > 0x86 || (f_label & 1u) != 0) {
        return 0xffff;
    }
    const std::uint32_t f = f_label - 0x80;
    std::uint32_t bit = 0;
    if (p_label >= 0x80 && p_label <= 0x86 && (p_label & 1u) == 0) {
        const std::uint32_t s = (p_label - 0x80) / 2;
        const std::uint32_t t = edge_of_dir(f);
        if (s == t) {
            return 0xffff; // a reversal cannot occur on a path
        }
        bit = pair_bit(s, t);
    } else {
        bit = 12u + f / 2u; // the start cell: the nibble bit
    }
    return static_cast<std::uint16_t>(~(1u << bit) & 0xffffu);
}

RouteResult plan_route_perturbed(const CoastIndex& index, const RouteQuery& query,
                                 const RouteLimits& limits, const RouteCostTable& costs,
                                 const Perturbation& perturbation) {
    Planner planner(index, query, limits, costs, perturbation);
    return planner.run();
}

} // namespace route_detail

RouteResult plan_route(const CoastIndex& index, const RouteQuery& query, const RouteLimits& limits,
                       const RouteCostTable& costs) {
    return route_detail::plan_route_perturbed(index, query, limits, costs,
                                              route_detail::Perturbation{});
}

} // namespace anno::sim
