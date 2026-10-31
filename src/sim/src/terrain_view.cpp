#include <anno/sim/terrain_view.hpp>

namespace anno::sim {

namespace {

/// The digest of a terrain with no columns at all, computed once. A terrain whose
/// columns are empty and whose digest differs is a hash-only placeholder.
std::uint64_t empty_terrain_digest() {
    static const std::uint64_t digest = terrain_digest(Terrain{});
    return digest;
}

} // namespace

VertexGrid VertexGrid::of(const Island& island) {
    VertexGrid g;
    g.x0_ = island.tiles.x0();
    g.y0_ = island.tiles.y0();
    g.w_ = island.tiles.width();
    g.h_ = island.tiles.height();
    const Terrain* t = island.terrain.get();
    if (t == nullptr) {
        g.status_ = VertexGridStatus::NoTerrain;
        return g;
    }
    const std::uint64_t want = std::uint64_t{g.w_} * g.h_;
    if (t->height.empty() && want != 0) {
        // An absent HEIGHT column, or the flat-stream placeholder (every column empty,
        // the digest of real ones). Another non-empty column, or the digest of absent
        // columns, means the column is genuinely absent; no scan is needed.
        const bool all_empty = t->gfx.empty() && t->fertility.empty();
        g.status_ = (!all_empty || t->digest == empty_terrain_digest())
                        ? VertexGridStatus::HeightColumnAbsent
                        : VertexGridStatus::HashOnlyPlaceholder;
        return g;
    }
    if (t->height.size() != want) {
        g.status_ = VertexGridStatus::HeightColumnWrongSize;
        return g;
    }
    g.height_ = t->height.data();
    g.status_ = VertexGridStatus::Ok;
    return g;
}

bool VertexGrid::contains(std::int64_t vx, std::int64_t vy) const {
    if (vx < x0_ || vy < y0_) {
        return false;
    }
    // vx >= x0_ and vy >= y0_: the differences are non-negative and fit in u64, so the
    // unsigned subtraction is exact for every i64 input (no signed overflow).
    const std::uint64_t lx =
        static_cast<std::uint64_t>(vx) - static_cast<std::uint64_t>(std::int64_t{x0_});
    const std::uint64_t ly =
        static_cast<std::uint64_t>(vy) - static_cast<std::uint64_t>(std::int64_t{y0_});
    return lx <= w_ && ly <= h_;
}

std::optional<std::uint8_t> VertexGrid::at(std::int64_t vx, std::int64_t vy) const {
    if (status_ != VertexGridStatus::Ok || !contains(vx, vy)) {
        return std::nullopt;
    }
    const std::uint64_t lx =
        static_cast<std::uint64_t>(vx) - static_cast<std::uint64_t>(std::int64_t{x0_});
    const std::uint64_t ly =
        static_cast<std::uint64_t>(vy) - static_cast<std::uint64_t>(std::int64_t{y0_});
    if (lx == w_ || ly == h_) {
        return kBoundaryVertexHeight;
    }
    return height_[ly * w_ + lx];
}

std::optional<std::uint64_t> VertexGrid::runtime_index(std::int64_t vx, std::int64_t vy) const {
    if (!contains(vx, vy)) {
        return std::nullopt;
    }
    return (static_cast<std::uint64_t>(vy) - static_cast<std::uint64_t>(std::int64_t{y0_})) *
               stride() +
           (static_cast<std::uint64_t>(vx) - static_cast<std::uint64_t>(std::int64_t{x0_}));
}

} // namespace anno::sim
