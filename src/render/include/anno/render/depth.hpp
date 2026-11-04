// Per-pixel depth as the original's depth buffer (docs/render/depth.md).
//
// The original draws into a u16 depth buffer: u16 = floor(65536 · d) with
// d = 0.5 − 0.00023663246 · (x + y) − 0.00018957623 · up at view rotation 0, smaller = nearer
// (D: AnnoDisplay.dll 0x100182f8–0x100183e3; the live camera matrix, docs/re/annodisplay.md
// §9.1). A sprite pixel's depth is the sprite's base plus its signed z byte (Z formats) or a
// row ramp (V and H formats); a pixel is drawn when its depth is at most the buffer's.
//
// This renderer keeps D256 = 256 × that unit in i32 (no 16-bit wrap) and computes it from a
// point's scene screen row and height in integers, so the CPU paths and gpu.wgsl (depth_at
// there) give the same values bit for bit.
#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <anno/formats/figuren.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/rect.hpp>

namespace anno::render::depth {

/// Original depth units (u16) per unit of world x + y: 65536 · 0.00023663246 (M: the camera
/// matrix's column 2, docs/re/annodisplay.md §9.1).
inline constexpr double kPerTileSum = 65536.0 * 0.00023663246;
/// Original depth units per HEIGHT unit of a world point: 65536 · 0.00018957623 · 0.04.
inline constexpr double kPerHeight = 65536.0 * 0.00018957623 * 0.04;
/// D256 of the depth plane's zero row (0.5 · 65536 · 256).
inline constexpr std::int64_t kZero = 8388608;
/// No depth here (no terrain drawn, nothing written): the original's 0xFFFF clear.
inline constexpr std::int32_t kNone = std::numeric_limits<std::int32_t>::max();

/// The integer constants of one zoom group (8 fractional bits).
struct Params {
    /// D256 per two scene rows, times 256: round(256 · 256 · kPerTileSum / th).
    std::int32_t ca = 0;
    /// D256 per h64 (1/64 HEIGHT unit) at a fixed scene row, times 256:
    /// round(256 · 4 · (2 · kPerTileSum · L / th + kPerHeight)), L = lift px per HEIGHT unit.
    std::int32_t cb = 0;
    /// The V formats' row ramp (an upright surface): D256 per row, times 256:
    /// round(256 · 256 · kPerHeight / L) (D: AnnoDisplay.dll 0x10014ca0, brush +4 = 0x3dc9 / 65536
    /// = 0.24135 per row at group 0, measured).
    std::int32_t sv = 0;
    /// The H formats' row ramp (a flat decal): the flat ground's D256 per row, times 256, −2 · ca
    /// (likely; the original's H step was not measured, docs/render/depth.md §1).
    std::int32_t sh = 0;
    bool operator==(const Params&) const = default;
};

/// The constants of `group` with the height scale `lift`.
Params params(std::size_t group, Lift lift);

/// D256 of the point at twice the scene row `y2` (2 · y for a point, 2 · y + 1 for a pixel
/// centre) and height h64 (1/64 HEIGHT units): kZero − floor((ca · y2 + cb · h64) / 256).
std::int32_t at(const Params& p, std::int64_t y2, std::int64_t h64);

/// at() split at a row origin, as the GPU evaluates it within a chunk: at(p, y2o + y2, h) ==
/// origin.base − floor_div(origin.frac + p.ca · y2 + p.cb · h, 256) for |y2|, h small enough
/// for i32 products (a chunk's rows).
struct Origin {
    std::int32_t base = 0;
    std::int32_t frac = 0;
};
Origin origin(const Params& p, std::int64_t y2o);

/// How a frame format meets the depth buffer.
enum class Class : std::uint8_t {
    /// No test, no write (shadows; units keep their former drawing).
    None = 0,
    /// base + 256 · z per pixel (RGBZ, RGBAZ, RGBZM, RGBWZ).
    Z = 1,
    /// base + the upright row ramp (RGBV, RGBAV).
    V = 2,
    /// base + the flat row ramp (RGBH, RGBAH).
    H = 3,
};
/// The format's class and write rule: whether its drawn pixels write (RGBZ, RGBZM, RGBV and
/// RGBAZ do; RGBAV, RGBH and RGBAH never), and whether its level-1 (50 %) pixels skip the write
/// (RGBAZ: D: AnnoDisplay.dll 0x100182f0).
struct Rule {
    Class cls = Class::None;
    bool writes = false;
    bool skip_half = false;
    bool operator==(const Rule&) const = default;
};
Rule rule(formats::figuren::FrameFormat format);

/// The alpha of a level-1 pixel (figure::kLevelAlpha[1]).
inline constexpr std::uint8_t kHalfAlpha = 128;

/// A pixel's depth: `row` counts from the frame's top row, `z` is the stored byte.
std::int32_t pixel(const Params& p, Class cls, std::int32_t base, std::int32_t row, std::uint8_t z);

/// The depth buffer of a scene rectangle while objects are drawn into it.
struct Target {
    Rect rect;
    Params params;
    /// Per pixel of `rect` (row-major) D256, kNone where nothing is.
    std::vector<std::int32_t> d;
    /// Per pixel: the last object pixel drawn here was depth-tested (1) or not (0).
    std::vector<std::uint8_t> tested;

    /// A target over `rect` from the terrain heights seen there (1/kTerrainScale HEIGHT units,
    /// kNoTerrain = none), as Chunk::terrain.
    static Target from_terrain(Rect rect, const Params& params,
                               const std::vector<std::uint16_t>& terrain);
    /// Index of scene pixel (sx, sy), or npos when outside.
    [[nodiscard]] std::size_t index(int sx, int sy) const;
    /// The submerged plane: 1 where the last drawn object pixel was tested and the scene depth
    /// there lies behind the sea plane at `sea_h64` (docs/render/depth.md §2.3); empty if none.
    [[nodiscard]] std::vector<std::uint8_t> submerged(std::int64_t sea_h64) const;
};

/// The sea plane's height in 1/64 HEIGHT units (Scene::shore_height).
std::int64_t sea_h64(float shore_height);

/// One sprite draw's depth: the base (D256 of its anchor point) and its format's rule.
struct Draw {
    std::int32_t base = 0;
    Rule rule;
    bool operator==(const Draw&) const = default;
};
/// The depth of the anchor at scene row `anchor_y` of an object `height` HEIGHT units up (its
/// item height byte), drawn with `format` (nullopt: no depth).
Draw draw(const Params& p, int anchor_y, std::uint8_t height,
          std::optional<formats::figuren::FrameFormat> format);

/// The test of image pixel (col, row) of a draw landing on scene pixel (sx, sy): false when the
/// target holds something nearer there (the pixel is not drawn); otherwise the pixel is drawn,
/// written when the rule says so, and marked tested. A pixel outside the target, or of a draw
/// without depth, passes.
bool test(Target& t, int sx, int sy, const Rgba& img, int col, int row, const Draw& d);
/// A drawn pixel of a draw without depth (a unit) over scene pixel (sx, sy): no longer tested.
void untest(Target& t, int sx, int sy);

/// draw_prepared (time `time_ms`, decoded variant 0, every pixel kept) with each object's
/// pixels tested against `t`; `offset` = scene position of `canvas`'s local point (0, 0).
DrawStats draw_prepared(Canvas& canvas, std::span<const Prepared> list,
                        const formats::anlagen::AnlagenFile& anlagen, std::size_t group, Lift lift,
                        std::uint64_t time_ms, SpriteCache& cache, Target& t,
                        std::pair<int, int> offset);

} // namespace anno::render::depth
