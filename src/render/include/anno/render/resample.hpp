// The resample pass of the continuous zoom (docs/render/iso.md "Continuous zoom"): the
// types shared by the GPU compositor (gpu.hpp, scale_fs in gpu.wgsl) and its CPU model,
// which the headless renders (--png --zoom-factor, bench_frame on the CPU) and the tests
// use as the reference. No GPU dependency.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include <anno/render/rect.hpp>

namespace anno::render {

/// Most samples per axis of a Box resample (and Area's loop limit in the shader): a scale
/// down to 1/8 averages 64 samples per output pixel (8 x 8); below that the picture would
/// skip grid pixels again, and a smaller scale needs mips of the grid.
inline constexpr std::uint32_t kMaxBoxTaps = 8;

/// How a resample pass reads the composed scene grid (T3, docs/render/iso.md
/// "Continuous zoom").
enum class ResampleFilter : std::uint32_t {
    /// The nearest scene pixel: sharp when magnifying (the pixel-art look).
    Nearest = 0,
    /// The four nearest scene pixels weighted by the position between them.
    Bilinear = 1,
    /// The mean of ceil(1 / scale) x ceil(1 / scale) nearest samples spread over
    /// the output pixel's footprint: for scales below 1 (minifying), where
    /// nearest would skip scene pixels and shimmer.
    Box = 2,
    /// The coverage-weighted mean of the scene pixels the output pixel's footprint
    /// (1 / scale wide) overlaps: at scales of 1 and above (magnifying) at most
    /// 2 x 2 pixels, so every output pixel is equally sharp and no row or column
    /// is doubled, as with Nearest at a fractional scale. Below 1 it is exact down
    /// to a scale of 1/7 (8 pixels per axis: the shader's loop limit); below that
    /// the weights are renormalised over the 8 x 8 pixels read (no darkening, but
    /// the picture undersamples again), so the viewer takes Box below 1.
    Area = 3,
};

/// The filter of the viewer's resample pass at `scale`: Box below 1 (reducing),
/// Bilinear from 1 (magnifying; the owner's choice at the stage 3 fix 1 look,
/// OQ-095 item 5). `forced` (ANNO_VIEW_RESAMPLE, the owner's A/B) overrides it, except
/// that Area below 1 becomes Box: Area reads at most 8 pixels per axis.
inline ResampleFilter resample_filter_for(double scale,
                                          std::optional<ResampleFilter> forced = std::nullopt) {
    if (forced) {
        return *forced == ResampleFilter::Area && scale < 1.0 ? ResampleFilter::Box : *forced;
    }
    return scale < 1.0 ? ResampleFilter::Box : ResampleFilter::Bilinear;
}

/// A continuous zoom: the picture is composed on the scene grid of a zoom
/// group, at one scene pixel per grid pixel, exactly as without a scale, and
/// one extra pass resamples it onto the target (`scale` target pixels per grid
/// pixel). The compositor, the chunks, the sea and the waves therefore never
/// see a fractional scale (their exactness tests keep their meaning); a scale
/// of 1 at offset 0 skips the pass altogether.
struct Resample {
    /// Target pixels per scene-grid pixel (> 0).
    double scale = 1.0;
    /// Where the target's left and top edge lie in the composed grid, in grid
    /// pixels from the view's origin (>= 0, usually < 1: the fraction of the
    /// camera position the integer view origin dropped).
    double offset_x = 0.0, offset_y = 0.0;
    ResampleFilter filter = ResampleFilter::Nearest;
    /// The size of the output (resampled_view sets it; 0 = not stated). The grid of a
    /// view is rounded up to kResampleGridStep, so at scale 1 and offset 0 the grid is
    /// still larger than the output and must be cut, not drawn onto it whole.
    std::uint32_t out_w = 0, out_h = 0;
    [[nodiscard]] bool identity() const {
        return scale == 1.0 && offset_x == 0.0 && offset_y == 0.0;
    }
    /// Whether a grid of `grid_w` x `grid_h` pixels is the output as it is.
    [[nodiscard]] bool is_noop_for(int grid_w, int grid_h) const {
        return identity() && (out_w == 0 || static_cast<int>(out_w) == grid_w) &&
               (out_h == 0 || static_cast<int>(out_h) == grid_h);
    }
};

/// The scene-grid view and the Resample for an output of `out_w` x `out_h` pixels
/// centred on the grid point (cx, cy) at `scale`: the integer rectangle that
/// covers what the output shows (plus one pixel for the filters' reach), and the
/// offset of the output's top-left corner in it.
struct ResampledView {
    Rect view;
    Resample resample;
};
/// The grid is rounded up to a multiple of kResampleGridStep in each direction so
/// that its size (and so the size of the textures composed into) stays the same
/// while the camera or the scale moves a little. `scale` is limited to
/// [1 / 64, 64] (a Box filter undersamples below 1 / 8 anyway) and raised when
/// the grid would not fit `max_grid` pixels (default: the 8192 every WebGPU
/// device offers as maxTextureDimension2D), so a very large window zooms in a
/// little more than asked instead of failing to create its textures.
inline constexpr int kResampleGridStep = 64;
ResampledView resampled_view(double cx, double cy, std::uint32_t out_w, std::uint32_t out_h,
                             double scale, ResampleFilter filter, std::uint32_t max_grid = 8192);

/// The resample pass on the CPU, the arithmetic of `scale_fs` in gpu.wgsl in float, in the
/// same order: `grid` (RGBA8, `grid_w` x `grid_h`, row-major) onto an `out_w` x `out_h`
/// RGBA8 target. The GPU result is within 1 per channel of it (the shader's float
/// rounding may move a sample across a pixel edge in a handful of pixels; tests bound
/// that). The 0RGB overload is for FrameRenderer frames (alpha 255 out).
void cpu_resample(const std::uint8_t* grid, int grid_w, int grid_h, const Resample& rs,
                  std::uint32_t out_w, std::uint32_t out_h, std::uint8_t* out);
void cpu_resample(const std::uint32_t* grid, int grid_w, int grid_h, const Resample& rs,
                  std::uint32_t out_w, std::uint32_t out_h, std::uint32_t* out);

} // namespace anno::render
