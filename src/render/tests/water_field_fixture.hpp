// A synthetic scene for the water field (no game files): two islands, runs of
// pixels alternating between them, pixels without an island and an island
// whose IslandWater is missing, positions inside, on and outside the grids.
// render_test.cpp checks sea::water_field over it against a digest recorded
// with the code before the T1f speedups (commit 7479a2a; docs/perf/README.md,
// "T1f item 2"), so a refactor of the field that changes any byte fails in CI.
// Uses only what that commit has (IslandWater's data members, water_field).
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <anno/render/sea.hpp>

namespace anno::render::test {

/// A ridge rising one unit per tile left to right, through the waterline and
/// the empty-cell threshold, with a river over the high ground. The 8 x 8
/// blocks of the last column hold no sea (an input here).
inline sea::IslandWater ridge_island() {
    sea::IslandWater iw;
    iw.w = 48;
    iw.h = 40;
    iw.heights.resize(iw.w * iw.h);
    for (std::size_t y = 0; y < iw.h; ++y) {
        for (std::size_t x = 0; x < iw.w; ++x) {
            const auto jitter = static_cast<std::uint8_t>((x * 7U + y * 13U) % 4U);
            iw.heights[y * iw.w + x] = static_cast<std::uint8_t>(x + jitter);
        }
    }
    const std::size_t cw = (iw.w + 7) / 8;
    iw.sea_chunk.assign(cw * ((iw.h + 7) / 8), true);
    for (std::size_t by = 0; by < (iw.h + 7) / 8; ++by) {
        iw.sea_chunk[by * cw + cw - 1] = false;
    }
    iw.open.assign(iw.w * iw.h, 0.5F);
    iw.shore.assign(iw.w * iw.h, 1.0F);
    iw.river.assign(iw.w * iw.h, std::numeric_limits<float>::quiet_NaN());
    for (std::size_t x = 30; x < 46; ++x) {
        iw.river[20 * iw.w + x] = static_cast<float>(iw.heights[20 * iw.w + x]) + 9.0F;
    }
    return iw;
}

/// A plateau at 40 with a basin falling to the sea floor and a one-corner pit,
/// openness and shore distance varying per corner, sea in every block.
inline sea::IslandWater basin_island() {
    sea::IslandWater iw;
    iw.w = 30;
    iw.h = 20;
    iw.heights.assign(iw.w * iw.h, 40);
    for (std::size_t y = 0; y < iw.h; ++y) {
        for (std::size_t x = 0; x < 12; ++x) {
            iw.heights[y * iw.w + x] = static_cast<std::uint8_t>(x * 3U + (y % 3U));
        }
    }
    iw.heights[10 * iw.w + 22] = 0;
    iw.sea_chunk.assign(((iw.w + 7) / 8) * ((iw.h + 7) / 8), true);
    iw.open.resize(iw.w * iw.h);
    iw.shore.resize(iw.w * iw.h);
    for (std::size_t i = 0; i < iw.w * iw.h; ++i) {
        iw.open[i] = static_cast<float>(i % 5U) * 0.25F;
        iw.shore[i] = 0.1F * static_cast<float>(i / iw.w) + 0.05F * static_cast<float>(i % iw.w);
    }
    iw.river.assign(iw.w * iw.h, std::numeric_limits<float>::quiet_NaN());
    return iw;
}

struct FieldScene {
    /// Island 0 and 1; island 2 has no IslandWater (nullptr).
    std::vector<sea::IslandWater> islands;
    Rect rect;
    sea::SceneProbe probe;
};

inline FieldScene field_scene() {
    FieldScene s;
    s.islands.push_back(ridge_island());
    s.islands.push_back(basin_island());
    s.rect = Rect{0, 0, 120, 80};
    const auto n = static_cast<std::size_t>(s.rect.w * s.rect.h);
    s.probe.island.assign(n, sea::kNoIsland);
    s.probe.pos.assign(n, {0.0F, 0.0F});
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t px = i % 120, py = i / 120;
        // Runs of five pixels: island 0, 1, none, the missing island 2, 0 again.
        const std::size_t k = (px / 5 + py * 3) % 5;
        const std::uint32_t island = k == 2 ? sea::kNoIsland : k == 3 ? 2U : k == 1 ? 1U : 0U;
        s.probe.island[i] = island;
        const sea::IslandWater& iw = s.islands[island == 1U ? 1U : 0U];
        // Positions from 2.25 tiles outside the grid to 2.25 beyond it.
        const float fpx = static_cast<float>(px), fpy = static_cast<float>(py);
        float x = -2.25F + std::fmod(fpx * 0.37F + fpy * 0.11F, static_cast<float>(iw.w) + 4.5F);
        float y = -2.25F + std::fmod(fpy * 0.29F + fpx * 0.07F, static_cast<float>(iw.h) + 4.5F);
        if (px % 13 == 0) {
            x = std::round(x); // exactly on a tile edge, also 0 and w
        }
        if (py % 11 == 0) {
            y = std::round(y);
        }
        s.probe.pos[i] = {x, y};
    }
    return s;
}

/// FNV-1a 64 of the four planes, one after the other.
inline std::uint64_t field_digest(const sea::WaterField& f) {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (const auto* plane : {&f.cover, &f.shore, &f.depth, &f.flags}) {
        for (const std::uint8_t b : *plane) {
            h ^= b;
            h *= 0x100000001b3ULL;
        }
    }
    return h;
}

} // namespace anno::render::test
