// Local-offset addressing of a TileLayer for the founding helpers (private to src/sim/src).
//
// A TileLayer accepts any 32-bit origin and a width/height whose far edge can lie above INT32_MAX
// (x0 = INT32_MAX, width 2 holds the world tiles INT32_MAX and INT32_MAX + 1). TileLayer::index_of
// takes 32-bit world coordinates and cannot name the second one. The founding helpers therefore
// take footprints in 64-bit world coordinates, check them against the layer ONCE, and address
// tiles by their local offset from the layer's origin: row-major index = ly * width + lx.
// Nothing here narrows a world coordinate. Supported domain: every footprint position an i64 can
// hold; a footprint not wholly inside the layer is reported as outside, never clipped or wrapped.
#pragma once

#include <cstdint>
#include <optional>

#include <anno/sim/island.hpp>

namespace anno::sim::detail {

/// The local offset of a footprint's first tile (both within the layer when returned).
struct LocalOrigin {
    std::uint64_t lx = 0, ly = 0;
};

/// The footprint (x, y, w, h) in world coordinates, as a local origin, or nullopt when it is not
/// wholly inside the layer (an empty w or h is inside when its corner is: the original's bounds
/// test looks at the corners; callers that refuse empty footprints say so themselves). Exact for
/// every i64 position (non-negative differences held in u64); a footprint reaching above INT32_MAX
/// in the layer's own range is accepted.
[[nodiscard]] inline std::optional<LocalOrigin> local_origin(const TileLayer& tiles, std::int64_t x,
                                                             std::int64_t y, std::uint32_t w,
                                                             std::uint32_t h) {
    if (x < tiles.x0() || y < tiles.y0()) {
        return std::nullopt;
    }
    const std::uint64_t lx =
        static_cast<std::uint64_t>(x) - static_cast<std::uint64_t>(std::int64_t{tiles.x0()});
    const std::uint64_t ly =
        static_cast<std::uint64_t>(y) - static_cast<std::uint64_t>(std::int64_t{tiles.y0()});
    if (lx > tiles.width() || w > tiles.width() - lx || ly > tiles.height() ||
        h > tiles.height() - ly) {
        return std::nullopt;
    }
    return LocalOrigin{lx, ly};
}

/// Row-major index of the local tile (lx, ly); the caller has established lx < width, ly < height.
[[nodiscard]] inline std::uint64_t local_index(const TileLayer& tiles, std::uint64_t lx,
                                               std::uint64_t ly) {
    return ly * tiles.width() + lx;
}

} // namespace anno::sim::detail
