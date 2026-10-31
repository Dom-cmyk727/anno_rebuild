#include <algorithm>

#include <anno/sim/territory.hpp>

#include "local_tiles.hpp"

namespace anno::sim::territory {

namespace {

/// Paints the half-open LOCAL column span [lxl, lxr) of local row ly, neutral tiles only, clipped
/// to the layer. Local offsets are small non-negative-or-clipped i64 (bounded by the layer size
/// plus the radius), so no world coordinate is narrowed. Returns the number of tiles changed.
std::uint64_t paint_row(TileLayer& tiles, std::uint32_t neutral, std::uint32_t city,
                        std::int64_t ly, std::int64_t lxl, std::int64_t lxr) {
    if (ly < 0 || ly >= std::int64_t{tiles.height()}) {
        return 0;
    }
    lxl = std::max<std::int64_t>(lxl, 0);
    lxr = std::min<std::int64_t>(lxr, tiles.width());
    if (lxl >= lxr) {
        return 0;
    }
    std::uint64_t idx =
        detail::local_index(tiles, static_cast<std::uint64_t>(lxl), static_cast<std::uint64_t>(ly));
    std::uint64_t changed = 0;
    for (std::int64_t x = lxl; x < lxr; ++x, ++idx) {
        if (tiles.owner(idx) == neutral) {
            tiles.set_owner(idx, city);
            ++changed;
        }
    }
    return changed;
}

/// The footprint's local origin, or nullopt when it is empty or not wholly inside the layer.
std::optional<detail::LocalOrigin> fit(const TileLayer& tiles, const Footprint& f) {
    if (f.w == 0 || f.h == 0) {
        return std::nullopt;
    }
    return detail::local_origin(tiles, f.x, f.y, f.w, f.h);
}

} // namespace

std::optional<std::vector<std::uint32_t>> disc_profile(std::uint32_t radius,
                                                       std::uint32_t max_radius) {
    if (radius > max_radius) {
        return std::nullopt;
    }
    if (radius == 0) {
        return std::vector<std::uint32_t>{};
    }
    // The loop of AnnoWelt.dll 0x100858c8..: a Bresenham-style walk that fills T[ecx] on the way
    // up and T[eax] on the way down. u64 here (no value wraps: run >= 0 and edx = (radius - j) *
    // (radius - j - 1)); the original's byte-wide storage limits equality to radius <= 126
    // (territory.hpp).
    const std::uint64_t r = radius;
    std::vector<std::uint32_t> t(r + 2, 0);
    std::uint64_t edx = (r - 1) * r;
    std::uint64_t eax = r;
    std::uint64_t ecx = 0;
    std::uint64_t run = r * r;
    std::uint64_t e10 = 2 * r;
    std::uint64_t e14 = 0;
    for (;;) {
        const std::uint64_t edi = run - 1 - e14;
        t[ecx] = static_cast<std::uint32_t>(eax);
        run = edi;
        if (!(edi > edx)) {
            e10 -= 2;
            t[eax] = static_cast<std::uint32_t>(ecx);
            eax -= 1;
            edx -= e10;
        }
        ++ecx;
        e14 += 2;
        if (!(ecx <= eax)) {
            break;
        }
    }
    if (radius <= 3) { // the original's correction for tiny radii
        for (std::uint64_t e = r - ((r + 1) / 4); e <= r; ++e) {
            t[e] += 1;
        }
    }
    t.resize(r + 1);
    return t;
}

Result claim_neutral_rect(TileLayer& tiles, std::uint32_t neutral_owner, std::uint32_t city_owner,
                          const Footprint& f) {
    const auto local = fit(tiles, f);
    if (!local) {
        return {Status::BadFootprint, 0};
    }
    if (city_owner == neutral_owner) {
        return {Status::InvalidOwner, 0};
    }
    Result res;
    const auto lx = static_cast<std::int64_t>(local->lx);
    const auto ly = static_cast<std::int64_t>(local->ly);
    for (std::uint32_t dy = 0; dy < f.h; ++dy) {
        res.claimed +=
            paint_row(tiles, neutral_owner, city_owner, ly + dy, lx, lx + std::int64_t{f.w});
    }
    return res;
}

Result claim_neutral_disc(TileLayer& tiles, std::uint32_t neutral_owner, std::uint32_t city_owner,
                          const Footprint& f, std::uint32_t radius, std::uint32_t max_radius) {
    const auto local = fit(tiles, f);
    if (!local) {
        return {Status::BadFootprint, 0};
    }
    if (city_owner == neutral_owner) {
        return {Status::InvalidOwner, 0};
    }
    if (radius > max_radius) {
        return {Status::RadiusTooLarge, 0};
    }
    if (radius == 0) {
        return claim_neutral_rect(tiles, neutral_owner, city_owner, f);
    }
    const std::vector<std::uint32_t> profile = *disc_profile(radius, max_radius);
    const std::int64_t hx = (std::int64_t{f.w} - 1) / 2;
    const std::int64_t hy = (std::int64_t{f.h} - 1) / 2;
    const std::int64_t px = (std::int64_t{f.w} - 1) & 1;
    const std::int64_t py = (std::int64_t{f.h} - 1) & 1;
    // Centre in LOCAL coordinates (the footprint is inside the layer, so these are small).
    const auto cx = static_cast<std::int64_t>(local->lx) + hx;
    const auto cy = static_cast<std::int64_t>(local->ly) + hy;
    Result res;
    // Each k visits two rows; with py == 0 the two rows of k == 0 are the same row (a second
    // visit finds no neutral tile left, so nothing is counted twice).
    for (std::uint32_t k = 0; k <= radius; ++k) {
        const std::int64_t t = profile[k];
        const std::int64_t xl = cx - t;
        const std::int64_t xr = cx + px + t + 1;
        res.claimed += paint_row(tiles, neutral_owner, city_owner, cy - k, xl, xr);
        res.claimed += paint_row(tiles, neutral_owner, city_owner, cy + py + k, xl, xr);
    }
    return res;
}

} // namespace anno::sim::territory
