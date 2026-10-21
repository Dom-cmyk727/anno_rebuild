#include <anno/render/resample.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace anno::render {

ResampledView resampled_view(double cx, double cy, std::uint32_t out_w, std::uint32_t out_h,
                             double scale, ResampleFilter filter, std::uint32_t max_grid) {
    // The grid's size is out / scale plus the filters' reach plus the rounding up to
    // kResampleGridStep (at most 2 + kResampleGridStep): the smallest scale that
    // fits max_grid.
    constexpr double kSlack = 2.0 + kResampleGridStep;
    const double fit = static_cast<double>(std::max(out_w, out_h)) /
                       std::max(static_cast<double>(max_grid) - kSlack, 1.0);
    scale = std::clamp(std::max(scale, fit), 1.0 / 64.0, 64.0);
    // The output's left/top edge in grid pixels, and its far edge.
    const double x0 = cx - static_cast<double>(out_w) / (2.0 * scale);
    const double y0 = cy - static_cast<double>(out_h) / (2.0 * scale);
    const double x1 = x0 + static_cast<double>(out_w) / scale;
    const double y1 = y0 + static_cast<double>(out_h) / scale;
    ResampledView r;
    r.view.x = static_cast<int>(std::floor(x0));
    r.view.y = static_cast<int>(std::floor(y0));
    // One more pixel than the far edge needs: the bilinear filter reads the next
    // pixel, and the box filter's footprint reaches half an output pixel past.
    auto round_up = [](int v) {
        return (v + kResampleGridStep - 1) / kResampleGridStep * kResampleGridStep;
    };
    r.view.w = round_up(static_cast<int>(std::ceil(x1)) - r.view.x + 1);
    r.view.h = round_up(static_cast<int>(std::ceil(y1)) - r.view.y + 1);
    r.resample.scale = scale;
    r.resample.offset_x = x0 - static_cast<double>(r.view.x);
    r.resample.offset_y = y0 - static_cast<double>(r.view.y);
    r.resample.filter = filter;
    return r;
}


namespace {

using Texel = std::array<float, 4>;

/// scale_fs of gpu.wgsl: the grid is read through `texel(x, y)` (clamped to the grid), the
/// values are channel values 0..255 as floats; the arithmetic is the shader's, in the
/// shader's order. Returns the output pixel's four channels (not rounded).
template <class Fetch>
Texel resample_pixel(const Fetch& fetch, int grid_w, int grid_h, const Resample& rs,
                     std::uint32_t x, std::uint32_t y) {
    auto texel = [&](int tx, int ty) {
        return fetch(std::clamp(tx, 0, grid_w - 1), std::clamp(ty, 0, grid_h - 1));
    };
    auto mix = [](const Texel& a, const Texel& b, float t) {
        Texel r;
        for (std::size_t k = 0; k < 4; ++k) {
            r[k] = a[k] * (1.0F - t) + b[k] * t;
        }
        return r;
    };
    const auto scale = static_cast<float>(rs.scale);
    // pos.xy is the target pixel's centre; src its centre in grid pixels.
    const float sx = static_cast<float>(rs.offset_x) + (static_cast<float>(x) + 0.5F) / scale;
    const float sy = static_cast<float>(rs.offset_y) + (static_cast<float>(y) + 0.5F) / scale;
    switch (rs.filter) {
    case ResampleFilter::Bilinear: {
        const float fx = sx - 0.5F, fy = sy - 0.5F;
        const int ix = static_cast<int>(std::floor(fx)), iy = static_cast<int>(std::floor(fy));
        const float tx = fx - std::floor(fx), ty = fy - std::floor(fy);
        const Texel a = mix(texel(ix, iy), texel(ix + 1, iy), tx);
        const Texel b = mix(texel(ix, iy + 1), texel(ix + 1, iy + 1), tx);
        return mix(a, b, ty);
    }
    case ResampleFilter::Box: {
        const int n = static_cast<int>(
            std::clamp<std::uint32_t>(static_cast<std::uint32_t>(std::ceil(1.0 / rs.scale)), 1U,
                                      kMaxBoxTaps));
        const float span = 1.0F / scale;
        Texel sum{};
        for (int j = 0; j < n; ++j) {
            for (int i = 0; i < n; ++i) {
                const float dx = (static_cast<float>(i) + 0.5F) / static_cast<float>(n) - 0.5F;
                const float dy = (static_cast<float>(j) + 0.5F) / static_cast<float>(n) - 0.5F;
                const Texel t = texel(static_cast<int>(std::floor(sx + dx * span)),
                                      static_cast<int>(std::floor(sy + dy * span)));
                for (std::size_t k = 0; k < 4; ++k) {
                    sum[k] += t[k];
                }
            }
        }
        for (float& v : sum) {
            v /= static_cast<float>(n * n);
        }
        return sum;
    }
    case ResampleFilter::Area: {
        const float span = 1.0F / scale;
        const float lx = sx - 0.5F * span, hx = sx + 0.5F * span;
        const float ly = sy - 0.5F * span, hy = sy + 0.5F * span;
        const int x0 = static_cast<int>(std::floor(lx)), y0 = static_cast<int>(std::floor(ly));
        const int x1 = std::max(static_cast<int>(std::ceil(hx)) - 1, x0);
        const int y1 = std::max(static_cast<int>(std::ceil(hy)) - 1, y0);
        Texel sum{};
        float wsum = 0.0F;
        const int limit = static_cast<int>(kMaxBoxTaps);
        for (int j = 0; j < limit && y0 + j <= y1; ++j) {
            const int ty = y0 + j;
            const float wy = (std::min(hy, static_cast<float>(ty + 1)) -
                              std::max(ly, static_cast<float>(ty))) /
                             span;
            for (int i = 0; i < limit && x0 + i <= x1; ++i) {
                const int tx = x0 + i;
                const float wx = (std::min(hx, static_cast<float>(tx + 1)) -
                                  std::max(lx, static_cast<float>(tx))) /
                                 span;
                const Texel t = texel(tx, ty);
                for (std::size_t k = 0; k < 4; ++k) {
                    sum[k] += t[k] * (wx * wy);
                }
                wsum += wx * wy;
            }
        }
        for (float& v : sum) {
            v /= std::max(wsum, 1e-6F);
        }
        return sum;
    }
    case ResampleFilter::Nearest:
        break;
    }
    return texel(static_cast<int>(std::floor(sx)), static_cast<int>(std::floor(sy)));
}

std::uint8_t to_byte(float v) {
    return static_cast<std::uint8_t>(std::clamp(std::floor(v + 0.5F), 0.0F, 255.0F));
}

} // namespace

void cpu_resample(const std::uint8_t* grid, int grid_w, int grid_h, const Resample& rs,
                  std::uint32_t out_w, std::uint32_t out_h, std::uint8_t* out) {
    auto fetch = [&](int x, int y) {
        const std::uint8_t* p =
            grid + (static_cast<std::size_t>(y) * static_cast<std::size_t>(grid_w) +
                    static_cast<std::size_t>(x)) *
                       4;
        return Texel{static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2]),
                     static_cast<float>(p[3])};
    };
    for (std::uint32_t y = 0; y < out_h; ++y) {
        for (std::uint32_t x = 0; x < out_w; ++x) {
            const Texel v = resample_pixel(fetch, grid_w, grid_h, rs, x, y);
            std::uint8_t* o = out + (static_cast<std::size_t>(y) * out_w + x) * 4;
            for (std::size_t k = 0; k < 4; ++k) {
                o[k] = to_byte(v[k]);
            }
        }
    }
}

void cpu_resample(const std::uint32_t* grid, int grid_w, int grid_h, const Resample& rs,
                  std::uint32_t out_w, std::uint32_t out_h, std::uint32_t* out) {
    auto fetch = [&](int x, int y) {
        const std::uint32_t p = grid[static_cast<std::size_t>(y) * static_cast<std::size_t>(grid_w) +
                                     static_cast<std::size_t>(x)];
        return Texel{static_cast<float>((p >> 16) & 0xFFU), static_cast<float>((p >> 8) & 0xFFU),
                     static_cast<float>(p & 0xFFU), 255.0F};
    };
    for (std::uint32_t y = 0; y < out_h; ++y) {
        for (std::uint32_t x = 0; x < out_w; ++x) {
            const Texel v = resample_pixel(fetch, grid_w, grid_h, rs, x, y);
            out[static_cast<std::size_t>(y) * out_w + x] =
                static_cast<std::uint32_t>(to_byte(v[0])) << 16 |
                static_cast<std::uint32_t>(to_byte(v[1])) << 8 | to_byte(v[2]);
        }
    }
}

} // namespace anno::render
