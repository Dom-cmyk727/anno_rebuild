#include <algorithm>

#include <anno/sim/territory.hpp>

namespace anno::sim::territory {

namespace {

bool footprint_fits(const TileLayer& tiles, const Footprint& f) {
    if (f.w == 0 || f.h == 0) {
        return false;
    }
    // Exact for every i64 position: non-negative differences held in u64, no signed overflow.
    if (f.x < tiles.x0() || f.y < tiles.y0()) {
        return false;
    }
    const std::uint64_t lx =
        static_cast<std::uint64_t>(f.x) - static_cast<std::uint64_t>(std::int64_t{tiles.x0()});
    const std::uint64_t ly =
        static_cast<std::uint64_t>(f.y) - static_cast<std::uint64_t>(std::int64_t{tiles.y0()});
    return lx <= tiles.width() && f.w <= tiles.width() - lx && ly <= tiles.height() &&
           f.h <= tiles.height() - ly;
}

/// Paints the half-open column span [xl, xr) of world row y, neutral tiles only, clipped to
/// the layer. Returns the number of tiles changed.
std::uint64_t paint_row(TileLayer& tiles, std::uint32_t neutral, std::uint32_t city, std::int64_t y,
                        std::int64_t xl, std::int64_t xr) {
    const std::int64_t x0 = tiles.x0();
    const std::int64_t y0 = tiles.y0();
    if (y < y0 || y >= y0 + std::int64_t{tiles.height()}) {
        return 0;
    }
    xl = std::max(xl, x0);
    xr = std::min(xr, x0 + std::int64_t{tiles.width()});
    if (xl >= xr) {
        return 0;
    }
    std::uint64_t idx = tiles.index_of(static_cast<std::int32_t>(xl), static_cast<std::int32_t>(y));
    std::uint64_t changed = 0;
    for (std::int64_t x = xl; x < xr; ++x, ++idx) {
        if (tiles.owner(idx) == neutral) {
            tiles.set_owner(idx, city);
            ++changed;
        }
    }
    return changed;
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
    if (!footprint_fits(tiles, f)) {
        return {Status::BadFootprint, 0};
    }
    if (city_owner == neutral_owner) {
        return {Status::InvalidOwner, 0};
    }
    Result res;
    for (std::uint32_t dy = 0; dy < f.h; ++dy) {
        res.claimed +=
            paint_row(tiles, neutral_owner, city_owner, f.y + dy, f.x, f.x + std::int64_t{f.w});
    }
    return res;
}

Result claim_neutral_disc(TileLayer& tiles, std::uint32_t neutral_owner, std::uint32_t city_owner,
                          const Footprint& f, std::uint32_t radius, std::uint32_t max_radius) {
    if (!footprint_fits(tiles, f)) {
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
    const std::int64_t cx = f.x + hx;
    const std::int64_t cy = f.y + hy;
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
