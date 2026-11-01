// The coarse map of the route engine: limits and cost validation, the tile bitmap source, the
// 12-bit edge-connectivity packer (independent of the original's flood code, written from the
// documented bit layout and checked against the packer under emulation, fixtures/synthetic/sim/nav/
// packer_vectors.txt) and the world-level sector index.
#include <algorithm>
#include <limits>

#include <anno/sim/route.hpp>

namespace anno::sim {

namespace {

const std::uint32_t kOpenWord = 0xfffu; // mask 0xfff, type 0

std::string fail(const char* what) {
    return std::string(what);
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Limits and costs
// ---------------------------------------------------------------------------------------------

std::string validate(const RouteLimits& l) {
    if (l.window_radius_tiles < 1) {
        return fail("route limits: window_radius_tiles must be at least 1");
    }
    if (l.max_window_blocks < 1) {
        return fail("route limits: max_window_blocks must be at least 1");
    }
    if (l.goal_radius_tiles < 1 || l.goal_radius_tiles > 255) {
        return fail("route limits: goal_radius_tiles must be in 1..255");
    }
    if (l.run_cap < 1 || l.run_cap > 255) {
        return fail("route limits: run_cap must be in 1..255");
    }
    if (l.retry_cap > 255) {
        return fail("route limits: retry_cap must be at most 255");
    }
    if (l.unit_cap > 65535) {
        return fail("route limits: unit_cap must be at most 65535");
    }
    if (l.coarse_budget < 1) {
        return fail("route limits: coarse_budget must be at least 1");
    }
    if (l.fine_budget < 1) {
        return fail("route limits: fine_budget must be at least 1");
    }
    if (l.memory_bytes < 4096) {
        return fail("route limits: memory_bytes must be at least 4096");
    }
    return {};
}

RouteCostTable RouteCostTable::original() {
    RouteCostTable t;
    t.classes.assign(4, RouteClassCosts{});
    t.classes[3].shore_enterable = false;
    return t;
}

std::string validate(const RouteCostTable& table) {
    if (table.classes.empty()) {
        return fail("route costs: at least one navigation class is required");
    }
    for (std::size_t i = 0; i < table.classes.size(); ++i) {
        const RouteClassCosts& c = table.classes[i];
        const std::string prefix = "route costs: class " + std::to_string(i) + ": ";
        if (c.open_orth == 0 || c.open_diag == 0 || c.shore_orth == 0 || c.shore_diag == 0) {
            return prefix + "every cost must be positive";
        }
        if (c.open_orth > c.open_diag) {
            return prefix + "open_orth must not exceed open_diag (a turn costs the difference)";
        }
        if (c.shore_orth > c.shore_diag) {
            return prefix + "shore_orth must not exceed shore_diag (a turn costs the difference)";
        }
    }
    return {};
}

// ---------------------------------------------------------------------------------------------
// Tile sources
// ---------------------------------------------------------------------------------------------

CellClass classify_cell_by_tiles(const TileSource& src, std::int32_t cx, std::int32_t cz) {
    std::uint32_t water = 0;
    for (std::int32_t dz = 0; dz < 8; ++dz) {
        for (std::int32_t dx = 0; dx < 8; ++dx) {
            if (src.tile_is_water(cx * 8 + dx, cz * 8 + dz)) {
                ++water;
            }
        }
    }
    if (water == 64) {
        return CellClass::AllWater;
    }
    return water == 0 ? CellClass::AllLand : CellClass::Mixed;
}

CellClass TileSource::classify_cell(std::int32_t cx, std::int32_t cz) const {
    return classify_cell_by_tiles(*this, cx, cz);
}

TileMapSource::TileMapSource(std::int32_t width, std::int32_t height)
    : w_(std::max(width, 0)), h_(std::max(height, 0)) {
    water_.assign(static_cast<std::uint64_t>(w_) * static_cast<std::uint64_t>(h_), 1);
}

bool TileMapSource::tile_is_water(std::int32_t x, std::int32_t z) const {
    if (x < 0 || z < 0 || x >= w_ || z >= h_) {
        return false;
    }
    return water_[static_cast<std::uint64_t>(z) * static_cast<std::uint64_t>(w_) +
                  static_cast<std::uint64_t>(x)] != 0;
}

void TileMapSource::set_water(std::int32_t x, std::int32_t z, bool water) {
    if (x < 0 || z < 0 || x >= w_ || z >= h_) {
        return;
    }
    water_[static_cast<std::uint64_t>(z) * static_cast<std::uint64_t>(w_) +
           static_cast<std::uint64_t>(x)] = water ? 1 : 0;
}

void TileMapSource::fill_rect(std::int32_t x0, std::int32_t z0, std::int32_t x1, std::int32_t z1,
                              bool water) {
    for (std::int32_t z = std::max(z0, 0); z <= std::min(z1, h_ - 1); ++z) {
        for (std::int32_t x = std::max(x0, 0); x <= std::min(x1, w_ - 1); ++x) {
            set_water(x, z, water);
        }
    }
}

// ---------------------------------------------------------------------------------------------
// The edge-connectivity packer
// ---------------------------------------------------------------------------------------------

std::uint16_t pack_edge_mask(std::span<const std::uint8_t, 64> water) {
    std::array<bool, 64> seen{};
    std::uint32_t mask = 0;
    for (std::uint32_t first = 0; first < 64; ++first) {
        if (water[first] == 0 || seen[first]) {
            continue;
        }
        // One 4-connected water region; the edges it touches (bit 0 E, 1 N, 2 W, 3 S).
        std::uint32_t edges = 0;
        std::array<std::uint8_t, 64> stack{};
        std::uint32_t top = 0;
        stack[top++] = static_cast<std::uint8_t>(first);
        seen[first] = true;
        while (top > 0) {
            const std::uint32_t i = stack[--top];
            const std::uint32_t x = i & 7u;
            const std::uint32_t z = i >> 3;
            if (x == 7) {
                edges |= 1u;
            }
            if (z == 0) {
                edges |= 2u;
            }
            if (x == 0) {
                edges |= 4u;
            }
            if (z == 7) {
                edges |= 8u;
            }
            const auto visit = [&](std::uint32_t nx, std::uint32_t nz) {
                const std::uint32_t j = nx + 8u * nz;
                if (water[j] != 0 && !seen[j]) {
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
        for (std::uint32_t s = 0; s < 4; ++s) {
            if ((edges & (1u << s)) == 0) {
                continue;
            }
            for (std::uint32_t t = 0; t < 4; ++t) {
                if (t == s || (edges & (1u << t)) == 0) {
                    continue;
                }
                const std::uint32_t k = (s + 3u + 4u - t) & 3u;
                mask |= 1u << (3u * s + k);
            }
        }
    }
    return static_cast<std::uint16_t>(mask);
}

// ---------------------------------------------------------------------------------------------
// The goal disc table (0x1000d6a0)
// ---------------------------------------------------------------------------------------------

std::vector<std::uint8_t> disc_half_widths(std::int32_t r) {
    if (r < 0) {
        r = 0;
    }
    std::vector<std::uint8_t> buf(static_cast<std::uint64_t>(r) + 1, 0);
    const std::uint32_t ur = static_cast<std::uint32_t>(r);
    std::uint32_t a = ur * ur;
    std::uint32_t b = (ur - 1u) * ur;
    std::uint32_t d = 2u * ur;
    std::int32_t j = r;
    for (std::int32_t i = 0; i <= j; ++i) {
        buf[static_cast<std::uint64_t>(i)] = static_cast<std::uint8_t>(j);
        a = a - 1u - 2u * static_cast<std::uint32_t>(i);
        if (a <= b) { // unsigned comparison, as in the original (ja)
            d -= 2u;
            buf[static_cast<std::uint64_t>(j)] = static_cast<std::uint8_t>(i);
            --j;
            b -= d;
        }
    }
    if (r <= 3) {
        for (std::int32_t e = r - (r + 1) / 4; e <= r; ++e) {
            ++buf[static_cast<std::uint64_t>(e)];
        }
    }
    return buf;
}

// ---------------------------------------------------------------------------------------------
// CoastIndex
// ---------------------------------------------------------------------------------------------

std::uint32_t CoastIndex::build_cell(std::int32_t cx, std::int32_t cz) const {
    switch (source_->classify_cell(cx, cz)) {
    case CellClass::AllWater:
        return kOpenWord;
    case CellClass::AllLand:
        return 0;
    case CellClass::Mixed:
        break;
    }
    std::array<std::uint8_t, 64> tiles{};
    for (std::int32_t dz = 0; dz < 8; ++dz) {
        for (std::int32_t dx = 0; dx < 8; ++dx) {
            tiles[static_cast<std::uint64_t>(dx + 8 * dz)] =
                source_->tile_is_water(cx * 8 + dx, cz * 8 + dz) ? 1 : 0;
        }
    }
    return pack_edge_mask(std::span<const std::uint8_t, 64>(tiles));
}

std::uint32_t CoastIndex::cell_word(std::int32_t cx, std::int32_t cz) const {
    if (cx < 0 || cz < 0 || cx >= cells_x() || cz >= cells_z()) {
        return 0;
    }
    const std::uint64_t sector =
        static_cast<std::uint64_t>(cz / 8) * static_cast<std::uint64_t>(sectors_x_) +
        static_cast<std::uint64_t>(cx / 8);
    const auto& cells = sectors_[sector];
    if (cells.empty()) {
        return kOpenWord;
    }
    return cells[static_cast<std::uint64_t>((cz % 8) * 8 + (cx % 8))];
}

void CoastIndex::store_word(std::int32_t cx, std::int32_t cz, std::uint32_t w) {
    const std::uint64_t sector =
        static_cast<std::uint64_t>(cz / 8) * static_cast<std::uint64_t>(sectors_x_) +
        static_cast<std::uint64_t>(cx / 8);
    auto& cells = sectors_[sector];
    if (cells.empty()) {
        if (w == kOpenWord) {
            return;
        }
        cells.assign(64, kOpenWord);
    }
    cells[static_cast<std::uint64_t>((cz % 8) * 8 + (cx % 8))] = w;
    // Canonical form: a sector of 64 open cells is the null sector.
    if (std::all_of(cells.begin(), cells.end(), [](std::uint32_t v) { return v == kOpenWord; })) {
        cells.clear();
        cells.shrink_to_fit();
    }
}

CoastIndex::CoastIndex(const TileSource& source, std::span<const CellCoord> shore_cells)
    : source_(&source), w_(std::max(source.width(), 0)), h_(std::max(source.height(), 0)) {
    sectors_x_ = (cells_x() + 7) / 8;
    sectors_z_ = (cells_z() + 7) / 8;
    sectors_.assign(static_cast<std::uint64_t>(sectors_x_) * static_cast<std::uint64_t>(sectors_z_),
                    {});
    for (std::int32_t cz = 0; cz < cells_z(); ++cz) {
        for (std::int32_t cx = 0; cx < cells_x(); ++cx) {
            const std::uint32_t w = build_cell(cx, cz);
            if (w != kOpenWord) {
                store_word(cx, cz, w);
            }
        }
    }
    for (const CellCoord& c : shore_cells) {
        if (c.cx >= 0 && c.cz >= 0 && c.cx < cells_x() && c.cz < cells_z()) {
            store_word(c.cx, c.cz, cell_word(c.cx, c.cz) | (3u << 16));
        }
    }
}

void CoastIndex::rebuild_rect(TileRect rect, std::span<const CellCoord> shore_cells) {
    if (source_ == nullptr || rect.x1 <= rect.x0 || rect.z1 <= rect.z0) {
        return;
    }
    std::vector<CellCoord> shore(shore_cells.begin(), shore_cells.end());
    std::sort(shore.begin(), shore.end());
    const std::int32_t cx0 = std::max(rect.x0, 0) / 8;
    const std::int32_t cz0 = std::max(rect.z0, 0) / 8;
    const std::int32_t cx1 = std::min((rect.x1 - 1) / 8, cells_x() - 1);
    const std::int32_t cz1 = std::min((rect.z1 - 1) / 8, cells_z() - 1);
    for (std::int32_t cz = cz0; cz <= cz1; ++cz) {
        for (std::int32_t cx = cx0; cx <= cx1; ++cx) {
            std::uint32_t w = build_cell(cx, cz);
            if (std::binary_search(shore.begin(), shore.end(), CellCoord{cx, cz})) {
                w |= 3u << 16;
            }
            store_word(cx, cz, w);
        }
    }
}

std::string CoastIndex::verify_against_clean_rebuild(std::span<const CellCoord> shore_cells) const {
    if (source_ == nullptr) {
        return "coast index: not built";
    }
    const CoastIndex clean(*source_, shore_cells);
    for (std::int32_t cz = 0; cz < cells_z(); ++cz) {
        for (std::int32_t cx = 0; cx < cells_x(); ++cx) {
            if (cell_word(cx, cz) != clean.cell_word(cx, cz)) {
                return "coast index: cell (" + std::to_string(cx) + "," + std::to_string(cz) +
                       ") is " + std::to_string(cell_word(cx, cz)) + ", a clean rebuild gives " +
                       std::to_string(clean.cell_word(cx, cz));
            }
        }
    }
    if (!same_words(clean)) {
        return "coast index: sector storage differs from the canonical form";
    }
    return {};
}

std::uint16_t CoastIndex::mask(std::int32_t cx, std::int32_t cz) const {
    return static_cast<std::uint16_t>(cell_word(cx, cz) & 0xffffu);
}

std::uint16_t CoastIndex::type(std::int32_t cx, std::int32_t cz) const {
    return static_cast<std::uint16_t>(cell_word(cx, cz) >> 16);
}

std::uint64_t CoastIndex::allocated_sectors() const {
    std::uint64_t n = 0;
    for (const auto& s : sectors_) {
        n += s.empty() ? 0 : 1;
    }
    return n;
}

bool CoastIndex::same_words(const CoastIndex& other) const {
    return w_ == other.w_ && h_ == other.h_ && sectors_ == other.sectors_;
}

} // namespace anno::sim
