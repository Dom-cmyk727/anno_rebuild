// The sea surface, drawn per pixel from a pre-rendered world pattern (spec:
// docs/render/iso.md, "Sea surface").
//
// The original draws the sea as sprites of entry kSeaAnlage on a staggered
// lattice (y even, x ≡ 2·((y/2) mod 2) (mod 4)), raised to kSeaLevel, with
// animation phase (5x + 3y) mod 32 (D: AnnoWelt.dll 0x1009f4c0). Lattice
// and phase repeat after kPeriodTiles tiles in world x and y, so the whole
// sea at one animation step is one periodic image (SeaPattern); a frame only
// looks pixels up. Where the sea may appear, and how much sea floor shows
// through, is static per pixel (WaterField). Lattice and phase use world
// tile coordinates (whether the original uses island-local ones is not
// traced, OQ-026 b).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

#include <anno/formats/anlagen.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/rect.hpp>

namespace anno::render::sea {

/// Period of the sea lattice and its phase in world tiles: a shift by
/// (p, −p) changes (5x + 3y) mod 32 by 2p, by (p, p) by 8p; both vanish for
/// p = 16, a multiple of the lattice spacing 4.
inline constexpr int kPeriodTiles = 16;
/// Shore distances in WaterField::shore are 1/32 tiles; 255 = far.
inline constexpr float kShoreScale = 32.0F;
/// Depths in WaterField::depth are 1/4 HEIGHT units around kDepthBias.
inline constexpr float kDepthScale = 4.0F;
/// Byte value of depth 0 in WaterField::depth.
inline constexpr float kDepthBias = 128.0F;
/// Width in tiles over which an island's sea floor fades into open water
/// towards the island border. Tuning value (OQ-026 b).
inline constexpr float kBorderFadeTiles = 8.0F;
/// River surface above the river objects' height byte (HEIGHT units),
/// tuned visually (OQ-026 b).
inline constexpr float kRiverLevel = 9.0F;
/// Share of height_noise() on river banks (the sea's waterline uses all of
/// it). Tuning value (not original).
inline constexpr float kRiverBankNoise = 0.5F;
/// Openness (of 254) of enhanced river water. Tuning value (not original).
inline constexpr std::uint8_t kRiverOpenness = 100;
/// Pixel step of the grid the enhanced waves are evaluated on, per zoom
/// group. Tuning value (PLAN §0.1, §0.2).
inline constexpr std::array<int, kGroups> kShadingStep = {4, 2, 2};

/// The sea at every animation step as a periodic screen image: scene pixel
/// (sx, sy) at the pattern's zoom group is pattern pixel (sx mod width,
/// sy mod height).
class SeaPattern {
  public:
    int width = 0, height = 0;
    /// Milliseconds per animation step (0 = static).
    std::uint64_t step_ms = 0;

    /// Renders the pattern of `group`; `water` is the open-water colour.
    /// nullopt if the sea entry or its frames of `group` are missing.
    static std::optional<SeaPattern> build(const formats::anlagen::AnlagenFile& anlagen,
                                           std::size_t group, Lift lift, Px water,
                                           SpriteCache& cache);

    [[nodiscard]] std::size_t step_count() const { return steps_.size(); }
    /// Step k: the sprite-only and the open-water image, row-major.
    [[nodiscard]] std::pair<std::span<const Px>, std::span<const Px>>
    step_images(std::size_t k) const {
        return {steps_[k].sprite, steps_[k].filled};
    }
    /// (current, next) step at time_ms and the fraction towards the next.
    [[nodiscard]] std::tuple<std::size_t, std::size_t, float> steps_at(std::uint64_t time_ms) const;
    /// Premultiplied 0–1 average of the 2 × 2 pattern pixels at (sx, sy).
    [[nodiscard]] std::array<float, 4> smoothed(std::span<const Px> img, int sx, int sy) const;
    [[nodiscard]] std::size_t index(int sx, int sy) const;

  private:
    struct Step {
        /// Sea sprites only (transparent between them).
        std::vector<Px> sprite;
        /// Sea sprites over the open-water colour.
        std::vector<Px> filled;
    };
    std::vector<Step> steps_;
};

/// Static per-pixel sea data of a scene rectangle.
struct WaterField {
    Rect rect;
    /// 0 = no sea here; 1 + 254·o = sea with openness o ∈ [0, 1].
    std::vector<std::uint8_t> cover;
    /// Distance to the shore in 1/kShoreScale tiles, 255 = far.
    std::vector<std::uint8_t> shore;
    /// kDepthBias + depth · kDepthScale, clamped; 255 = deep or open sea.
    std::vector<std::uint8_t> depth;
    /// Bit 0: water in the original. Bit 1: enhanced river water. Bits 2–7:
    /// beach slope (sea) or river-surface slope (river) in 1/2 HEIGHT units
    /// per tile (slope_of).
    std::vector<std::uint8_t> flags;
};

/// Terrain height of the enhanced waterline at tile position (x, y): a cubic
/// B-spline through the corner heights plus fixed value noise.
float smooth_height(std::span<const std::uint8_t> heights, std::size_t w, std::size_t h, float x,
                    float y);

/// The fixed value noise smooth_height adds (HEIGHT units).
float height_noise(float x, float y);

/// Bed height under river water at tile position (x, y): a Catmull-Rom
/// surface through the corner heights, clamped to the four corners of the
/// cell. Unlike smooth_height (an approximating B-spline, right for the
/// waterline of the sea) it passes through every corner, so the floor of a
/// channel one corner wide keeps its depth.
float river_bed_height(std::span<const std::uint8_t> heights, std::size_t w, std::size_t h, float x,
                       float y);

/// A river piece: tile (x, y), footprint (w, h), height byte.
using RiverPiece =
    std::tuple<std::uint16_t, std::uint16_t, std::uint8_t, std::uint8_t, std::uint8_t>;

/// Per-island inputs of water_field, computed once per island.
class IslandWater {
  public:
    /// nullopt if the island's HEIGHT or GFXID grid cannot be read.
    static std::optional<IslandWater> build(const formats::insel::Island& island,
                                            std::span<const RiverPiece> rivers);
    /// River surface level at tile (tx, ty) + (fx, fy), nullopt away from
    /// rivers.
    [[nodiscard]] std::optional<float> river_level(std::size_t tx, std::size_t ty, float fx,
                                                   float fy) const;
    [[nodiscard]] float bilinear(std::span<const float> field, std::size_t tx, std::size_t ty,
                                 float fx, float fy) const;

    std::size_t w = 0, h = 0;
    std::vector<std::uint8_t> heights;
    /// Per 8 × 8 tile chunk: contains a non-land tile (OQ-026 b).
    std::vector<bool> sea_chunk;
    /// Openness per height corner (approximation, OQ-026 b).
    std::vector<float> open;
    /// Smoothed shore distance per height corner in tiles.
    std::vector<float> shore;
    /// River surface level per corner, NaN away from rivers.
    std::vector<float> river;
};

/// The ground seen at each pixel of a scene rectangle: island index
/// (kNoIsland where no ground was drawn) and the visible tile position.
struct SceneProbe {
    std::vector<std::uint32_t> island;
    std::vector<std::array<float, 2>> pos;
};
inline constexpr std::uint32_t kNoIsland = 0xFFFFFFFFU;

/// The ground at tile position (x, y) as the water field sees it: terrain
/// height, the tile and fraction, whether the original draws sea here and
/// the enhanced waterline distance `sd` (HEIGHT units above the smooth
/// terrain; sd > 0 = enhanced sea). Shared by water_field and the river
/// flow raster (river.hpp), so both classify the same way.
struct TileGround {
    float under = 0, smooth = 0, sd = 0, fx = 0, fy = 0;
    std::size_t tx = 0, ty = 0;
    bool sea_chunk = false, original_water = false;
};
TileGround tile_ground(const IslandWater& iw, float x, float y);

/// Enhanced river water at tile position (x, y) where the island has a river
/// level: the surface level, the bed (Catmull-Rom plus half the shore noise)
/// and the depth between them; nullopt where no level exists or the bed lies
/// above the level by more than the soft edge. The caller checks the ground
/// (not original water, sd <= 0), as water_field does.
struct RiverPoint {
    float level = 0, bed = 0, depth = 0;
};
std::optional<RiverPoint> river_point(const IslandWater& iw, float x, float y, const TileGround& g);

/// The water field of `rect` from the ground seen at each pixel; water(i)
/// gives the IslandWater of island i (nullptr if none).
WaterField water_field(Rect rect, const SceneProbe& probe,
                       const std::function<const IslandWater*(std::size_t)>& water);

/// Beach slope (HEIGHT units per tile) stored in water-field flags.
inline float slope_of(std::uint8_t flags) {
    return static_cast<float>(flags >> 2) * 0.5F;
}

/// A straight-alpha RGBA byte buffer the sea can be drawn onto.
struct RgbaTarget {
    std::uint8_t* data;
    [[nodiscard]] Px get(std::size_t i) const {
        return {data[i * 4], data[i * 4 + 1], data[i * 4 + 2], data[i * 4 + 3]};
    }
    void set(std::size_t i, Px c) const {
        for (std::size_t k = 0; k < 4; ++k) {
            data[i * 4 + k] = c[k];
        }
    }
};

/// An opaque 0RGB buffer (window frames) the sea can be drawn onto.
struct XrgbTarget {
    std::uint32_t* data;
    [[nodiscard]] Px get(std::size_t i) const {
        const std::uint32_t p = data[i];
        return {static_cast<std::uint8_t>(p >> 16), static_cast<std::uint8_t>(p >> 8),
                static_cast<std::uint8_t>(p), 255};
    }
    void set(std::size_t i, Px c) const {
        data[i] = std::uint32_t{c[0]} << 16 | std::uint32_t{c[1]} << 8 | std::uint32_t{c[2]};
    }
};

/// (cover, shore, depth, flags) of a region pixel.
using FieldPx = std::array<std::uint8_t, 4>;

/// Straight-alpha RGBA from premultiplied 0–1 values.
Px unpremultiply(std::array<float, 4> p);
/// (a · (scale − t) + b · t) / scale per channel.
Px lerp4(Px a, Px b, std::uint32_t t, std::uint32_t scale);

/// Draws the sea over `out` (RgbaTarget or XrgbTarget pointing at the
/// region's first pixel), a block of region.w × region.h pixels with row
/// stride `stride` showing scene rectangle `region` at `group`. field(i)
/// gives pixel i of the region (row-major). Original draws the pattern step
/// of time_ms; Enhanced also blends steps, hides the floor in deep water and
/// adds waves (not in the original).
template <class T, class F>
void shade_sea(T out, std::size_t stride, Rect region, F&& field, const SeaPattern& pattern,
               std::uint64_t time_ms, WaterStyle style, std::size_t group, Lift lift);

/// What shade_sea needs to draw enhanced rivers with the flow shading
/// (rivers.md 8): the layer table and the weight of valid texels below which
/// a pixel keeps the legacy shimmer.
struct RiverShading {
    const RiverParams* params = nullptr;
    float min_valid = 0.02F;
};

/// The constants of the river water plane at `group` (rivers.md E19): the tile
/// size and the pixels per HEIGHT unit of the lift (the lift scales with the
/// zoom group's tile width).
inline RiverView river_view(std::size_t group, Lift lift) {
    const auto twf = static_cast<float>(kTile[group].w), thf = static_cast<float>(kTile[group].h);
    return {twf, thf,
            static_cast<float>(lift.num) / static_cast<float>(lift.den) * twf /
                static_cast<float>(kTile[0].w)};
}

/// The look of a river pixel near the sea (Scene::blend_rivers_into_sea; rivers.md
/// 6, Q8): the sea's shade where the shore byte says kb = 0 (the river's mouth),
/// the river's where kb = 1; every component interpolates, so the aeration of the
/// river (the sea has none) fades out with it as the light and the tint do.
inline Shade river_sea_mix(const Shade& river, const Shade& sea, float kb) {
    return {sea.light + (river.light - sea.light) * kb, sea.foam + (river.foam - sea.foam) * kb,
            sea.tint + (river.tint - sea.tint) * kb,
            sea.aeration + (river.aeration - sea.aeration) * kb};
}

/// The shade of an enhanced river pixel (scene pixel sx, sy): the flow shading
/// where the lattice has a flow (valid >= rivers->min_valid), else the legacy
/// shimmer at `legacy_pos` (the sea plane's tile position). `rivers` null =
/// the legacy shimmer.
inline Shade river_pixel_shade(const RiverShading* rivers, const FlowSample& fs, int sx, int sy,
                               float depth, float slope, std::uint64_t time_ms, float detail,
                               const RiverView& view, std::pair<float, float> legacy_pos) {
    if (rivers != nullptr && rivers->params != nullptr && fs.valid >= rivers->min_valid) {
        return river_flow_shade(static_cast<float>(sx), static_cast<float>(sy), fs, depth, slope,
                                time_ms, detail, *rivers->params, view);
    }
    return river_shade(legacy_pos, depth, slope, time_ms);
}

/// shade_sea with the river flow: flow(i) gives the FlowSample of region
/// pixel i (called for enhanced river pixels only); `rivers` null = the
/// legacy shimmer everywhere.
template <class T, class F, class FL>
void shade_sea_flow(T out, std::size_t stride, Rect region, F&& field, FL&& flow,
                    const RiverShading* rivers, const SeaPattern& pattern, std::uint64_t time_ms,
                    WaterStyle style, std::size_t group, Lift lift);

// --- template definition ---

template <class T, class F>
void shade_sea(T out, std::size_t stride, Rect region, F&& field, const SeaPattern& pattern,
               std::uint64_t time_ms, WaterStyle style, std::size_t group, Lift lift) {
    shade_sea_flow(
        out, stride, region, std::forward<F>(field), [](std::size_t) { return FlowSample{}; },
        nullptr, pattern, time_ms, style, group, lift);
}

template <class T, class F, class FL>
void shade_sea_flow(T out, std::size_t stride, Rect region, F&& field, FL&& flow,
                    const RiverShading* rivers, const SeaPattern& pattern, std::uint64_t time_ms,
                    WaterStyle style, std::size_t group, Lift lift) {
    const auto [k0, k1, frac] = pattern.steps_at(time_ms);
    const auto [a_sprite, a_filled] = pattern.step_images(k0);
    const auto [b_sprite, b_filled] = pattern.step_images(k1);
    const auto rw = static_cast<std::size_t>(region.w), rh = static_cast<std::size_t>(region.h);
    const int step = kShadingStep[group];
    const int gx0 = div_euclid(region.x, step) * step;
    const int gy0 = div_euclid(region.y, step) * step;
    const auto gw = static_cast<std::size_t>(div_euclid(region.x + region.w - gx0, step) + 2);
    const auto gh = static_cast<std::size_t>(div_euclid(region.y + region.h - gy0, step) + 2);
    const int raise = lift.scaled(kSeaLevel, 1, group);
    const float twf = static_cast<float>(kTile[group].w);
    const float thf = static_cast<float>(kTile[group].h);
    // The water plane of the flow shading (rivers.md E19).
    const RiverView view = river_view(group, lift);
    const float river_detail = wave_detail(twf);
    std::vector<Shade> grid;
    if (style == WaterStyle::Enhanced) {
        const float detail = wave_detail(twf);
        grid.reserve(gw * gh);
        for (std::size_t j = 0; j < gh; ++j) {
            for (std::size_t i = 0; i < gw; ++i) {
                const int sx = gx0 + step * static_cast<int>(i);
                const int sy = gy0 + step * static_cast<int>(j);
                const auto cx =
                    static_cast<std::size_t>(std::clamp(sx - region.x, 0, region.w - 1));
                const auto cy =
                    static_cast<std::size_t>(std::clamp(sy - region.y, 0, region.h - 1));
                // Land points too (shore 0), so foam reaches the beach.
                const FieldPx f = field(cy * rw + cx);
                const float px = static_cast<float>(sx), py = static_cast<float>(sy + raise);
                const std::pair<float, float> pos{px / twf + py / thf, py / thf - px / twf};
                const float depth = (static_cast<float>(f[2]) - kDepthBias) / kDepthScale;
                grid.push_back(wave_shade(static_cast<float>(f[1]) / kShoreScale, depth,
                                          slope_of(f[3]), pos, time_ms, detail));
            }
        }
    }
    const float stepf = static_cast<float>(step);
    for (std::size_t row = 0; row < rh; ++row) {
        const int sy = region.y + static_cast<int>(row);
        for (std::size_t col = 0; col < rw; ++col) {
            const FieldPx f = field(row * rw + col);
            const std::uint8_t cover = f[0], flags = f[3];
            if (cover == 0) {
                continue;
            }
            const float sd = (static_cast<float>(f[2]) - kDepthBias) / kDepthScale;
            const int sx = region.x + static_cast<int>(col);
            float alpha = 1.0F;
            if (style == WaterStyle::Original) {
                if ((flags & 1U) == 0) {
                    continue;
                }
            } else {
                const Waves& w = kWaves;
                // The waterline moves with the swash (not on rivers).
                float run = 0.0F;
                if (sd < w.swash_depth && (flags & 2U) == 0) {
                    const float px = static_cast<float>(sx), py = static_cast<float>(sy + raise);
                    run = swash_runup(static_cast<float>(f[1]) / kShoreScale, slope_of(flags),
                                      {px / twf + py / thf, py / thf - px / twf}, time_ms);
                }
                alpha = smoothstep(-w.shore_edge, w.shore_edge, sd + run);
            }
            if (alpha <= 0.0F) {
                continue;
            }
            const std::size_t i = pattern.index(sx, sy);
            Px sprite = a_sprite[i], filled = a_filled[i];
            if (style == WaterStyle::Enhanced) {
                // 2 × 2 average of the pattern, blended into the next step.
                auto mix = [frac](std::array<float, 4> p, std::array<float, 4> q) {
                    std::array<float, 4> r{};
                    for (std::size_t c = 0; c < 4; ++c) {
                        r[c] = p[c] + (q[c] - p[c]) * frac;
                    }
                    return r;
                };
                sprite = unpremultiply(
                    mix(pattern.smoothed(a_sprite, sx, sy), pattern.smoothed(b_sprite, sx, sy)));
                filled = unpremultiply(
                    mix(pattern.smoothed(a_filled, sx, sy), pattern.smoothed(b_filled, sx, sy)));
            }
            const std::size_t dst = row * stride + col;
            const Px below = out.get(dst);
            Px c = below;
            over(c, sprite);
            std::uint32_t open = cover - 1U;
            if (style == WaterStyle::Enhanced) {
                const auto deep = sat_cast<std::uint32_t>(deep_water(fmaxf(sd, 0.0F)) * 254.0F);
                open = std::max(open, deep);
            }
            if (open > 0) {
                c = lerp4(c, filled, open, 254);
            }
            if (style == WaterStyle::Enhanced) {
                const int gx = div_euclid(sx - gx0, step), gy = div_euclid(sy - gy0, step);
                const float fx = static_cast<float>((sx - gx0) - gx * step) / stepf;
                const float fy = static_cast<float>((sy - gy0) - gy * step) / stepf;
                auto at = [&](int x, int y) -> const Shade& {
                    return grid[static_cast<std::size_t>(y) * gw + static_cast<std::size_t>(x)];
                };
                const Shade &p00 = at(gx, gy), &p10 = at(gx + 1, gy), &p01 = at(gx, gy + 1),
                            &p11 = at(gx + 1, gy + 1);
                auto blend = [&](float Shade::* m) {
                    return (p00.*m * (1.0F - fx) + p10.*m * fx) * (1.0F - fy) +
                           (p01.*m * (1.0F - fx) + p11.*m * fx) * fy;
                };
                Shade shade;
                if ((flags & 2U) == 0) {
                    shade = {blend(&Shade::light), blend(&Shade::foam), blend(&Shade::tint), 0.0F};
                } else {
                    const float px = static_cast<float>(sx), py = static_cast<float>(sy + raise);
                    const std::pair<float, float> pos{px / twf + py / thf, py / thf - px / twf};
                    // The flow shading where the lattice has a flow, else the
                    // legacy shimmer.
                    const FlowSample fs = rivers != nullptr ? flow(row * rw + col) : FlowSample{};
                    shade = river_pixel_shade(rivers, fs, sx, sy, sd, slope_of(flags), time_ms,
                                              river_detail, view, pos);
                    // Near the sea the river takes on the sea's look
                    // (Scene::blend_rivers_into_sea; the GPU uses the wave
                    // simulation there when it runs).
                    const float kb = fminf(static_cast<float>(f[1]) / 254.0F, 1.0F);
                    if (kb < 1.0F) {
                        const Shade s =
                            wave_shade(kRiverSeaDist, sd, 0.0F, pos, time_ms, wave_detail(twf));
                        shade = river_sea_mix(shade, s, kb);
                    }
                }
                c = shade_apply(c, shade);
            }
            if (alpha < 1.0F) {
                c = lerp4(below, c, sat_cast<std::uint32_t>(alpha * 256.0F), 256);
            }
            out.set(dst, c);
        }
    }
}

} // namespace anno::render::sea
