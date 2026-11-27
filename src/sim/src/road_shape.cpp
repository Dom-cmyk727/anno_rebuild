#include <algorithm>
#include <array>
#include <map>
#include <utility>

#include <anno/sim/road_shape.hpp>

namespace anno::sim {
namespace {

// N, E, S, W: mask bits 1, 2, 4, 8 (0x1007f16b: bit 1 << direction).
constexpr std::array<std::pair<int, int>, 4> kDirs{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};

TileXY step(TileXY t, std::size_t d) {
    return TileXY{t.x + kDirs[d].first, t.y + kDirs[d].second};
}

bool is_dirt(const RoadShapeTable& t, std::uint32_t id) {
    return id >= t.dirt_first && id <= t.dirt_last;
}

} // namespace

bool road_connects(const RoadShapeTable& t, std::uint32_t id) {
    return id >= t.road_first && id <= t.road_last;
}

std::optional<std::uint32_t> road_shaped_base(const RoadShapeTable& t, std::uint32_t id) {
    if (is_dirt(t, id)) {
        id -= t.dirt_offset;
    }
    if (id > t.cobble_first && id <= t.cobble_straight_last) {
        id = t.cobble_first;
    }
    if (std::find(t.shaped.begin(), t.shaped.end(), id) == t.shaped.end()) {
        return std::nullopt;
    }
    return id;
}

RoadShapeEntry road_shape(const RoadShapeTable& t, std::uint32_t family_id, unsigned mask) {
    RoadShapeEntry e = t.by_mask.at(mask);
    if (is_dirt(t, family_id)) {
        e.id += t.dirt_offset;
    }
    return e;
}

std::vector<ShapedRoad> reshape_road_path(const RoadShapeTable& t, const std::vector<TileXY>& path,
                                          const std::vector<std::uint32_t>& ids,
                                          const RoadAt& existing) {
    std::map<TileXY, std::size_t> on_path;
    for (std::size_t i = 0; i < path.size(); ++i) {
        on_path.emplace(path[i], i);
    }
    const auto connects = [&](TileXY p) {
        if (on_path.count(p) != 0) {
            return true;
        }
        const auto o = existing ? existing(p) : std::nullopt;
        return o && road_connects(t, o->id);
    };
    const auto mask_at = [&](TileXY p) {
        unsigned m = 0;
        for (std::size_t d = 0; d < kDirs.size(); ++d) {
            m |= connects(step(p, d)) ? 1U << d : 0U;
        }
        return m;
    };
    // A straight keeps the id it has (its cosmetic variant, OQ-901); any other piece takes the
    // table's id in the tile's family.
    const auto shaped = [&](std::uint32_t family_id, unsigned mask) {
        const RoadShapeEntry e = road_shape(t, family_id, mask);
        const bool straight = road_shaped_base(t, e.id) == std::optional{t.cobble_first};
        const bool was_straight = road_shaped_base(t, family_id) == std::optional{t.cobble_first};
        return RoadShapeEntry{straight && was_straight ? family_id : e.id, e.rotation};
    };
    std::vector<ShapedRoad> out;
    out.reserve(path.size());
    for (std::size_t i = 0; i < path.size(); ++i) {
        const RoadShapeEntry e = shaped(ids.at(i), mask_at(path[i]));
        out.push_back({path[i], e.id, e.rotation});
    }
    std::map<TileXY, bool> seen;
    for (const TileXY& p : path) {
        for (std::size_t d = 0; d < kDirs.size(); ++d) {
            const TileXY n = step(p, d);
            if (on_path.count(n) != 0 || !seen.emplace(n, true).second || !existing) {
                continue;
            }
            const auto o = existing(n);
            if (!o || !road_shaped_base(t, o->id)) {
                continue; // not a piece the reshaper places (or no road at all)
            }
            const RoadShapeEntry e = shaped(o->id, mask_at(n));
            if (e.id != o->id || e.rotation != o->rotation) {
                out.push_back({n, e.id, e.rotation});
            }
        }
    }
    return out;
}

} // namespace anno::sim
