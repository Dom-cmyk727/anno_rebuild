// CPU isometric renderer for one island: ground and placed objects into an
// RGBA canvas. The reference implementation of the projection rules (and
// of the GPU path's checks).
//
// Projection rules (spec: docs/render/iso.md; derived from sprite
// measurements and visual checks, OQ-026):
// - Tile (x, y) is a diamond kTile[group] wide/high whose top vertex is at
//   screen ((x − y) · w/2, (x + y) · h/2).
// - Ground: the point with tile coordinates (s, t) ∈ [0, 1)² shows
//   GroundCell::texel (cells are 1/6 of a 256 × 256 Felder.tex texture).
// - Objects: the sprite's anchor is put on the centre of the footprint
//   diamond. Frame = rotation · animation frames + animation step.
// - Draw order: ground, then objects by the depth of their footprint's far
//   corner (x + w + y + h), then by x.
// - Elevation: HEIGHT values are heights at tile corners; tile (x, y) spans
//   corners (x, y), (x+1, y), (x, y+1), (x+1, y+1) as two triangles split
//   along the (x, y+1)–(x+1, y) diagonal and is raised by height · lift
//   pixels at zoom group 0 (kHeightLift is the original's scale).
// - Alpha levels blend; shadow frames darken to 5/8 before their sprite.
//
// The enhanced water (wave_shade and friends) is not in the original: an
// opt-in improvement (docs/render/iso.md, "Enhanced water").
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <anno/assets/island.hpp>
#include <anno/assets/texture.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/numeric.hpp>

namespace anno::render {

using assets::Rgba;
using Px = std::array<std::uint8_t, 4>;

/// Tile diamond (width, height) per zoom group (P: flat 1 × 1 Anlagen.dat
/// sprites are 84 × 42/43, 42 × 21/22 and 20 × 10). Three zoom groups in the
/// original; every per-group table is indexed by group and sized by this
/// one, to become content configuration (PLAN §0.1).
struct TileSize {
    int w, h;
};
inline constexpr std::array<TileSize, 3> kTile = {{{84, 42}, {42, 21}, {20, 10}}};
inline constexpr std::size_t kGroups = kTile.size();

/// An RGBA canvas whose pixel (0, 0) is screen point `origin`.
struct Canvas {
    std::size_t width = 0, height = 0;
    std::pair<int, int> origin{0, 0};
    std::vector<std::uint8_t> pixels;
    /// Optional (empty = not tracked): per pixel, how many rows the drawn
    /// object pixel lies above that object's lowest pixel in its column
    /// (0–254; 255 = no object). blit_sprite and composite keep it; the GPU
    /// compositor uses it to let enhanced water meet objects standing in it
    /// (docs/render/iso.md, "Water against objects").
    std::vector<std::uint8_t> base;
    /// Columns whose lowest pixel lies more than this many rows above the
    /// frame anchor (the ground point) do not touch the ground (foliage,
    /// rigging): base 255 there.
    int base_band = 0;

    /// A canvas covering the whole island at `group`, with a margin above.
    static Canvas for_island(const formats::insel::Island& island, std::size_t group, int margin);
    /// Draws `rgba` over the pixel (straight alpha, see over()).
    void put(int sx, int sy, Px rgba);
    /// Whether the screen rectangle (x, y, w, h) touches the canvas.
    [[nodiscard]] bool overlaps(int x, int y, int w, int h) const;
    [[nodiscard]] std::optional<std::size_t> index(int sx, int sy) const;
    /// The canvas as an image; nullopt if it is wider or higher than an Rgba
    /// can be (65,535 px; the Rust version panics there).
    [[nodiscard]] std::optional<Rgba> to_rgba() const;
};

/// A shadow pixel: 37.5 % black over the destination, the original's
/// dst/2 + dst/8 (5/8 brightness; D: AnnoDisplay.dll 0x10019f80) on an
/// opaque pixel. Overlapping shadows compound, as in tBrushShadR.
inline constexpr Px kShadow = {0, 0, 0, 96};

/// dst = src over dst for 8-bit straight-alpha RGBA (exact integer form in
/// units of 1/255²).
void over(std::uint8_t* dst, const std::uint8_t* src);
inline void over(Px& dst, const Px& src) {
    over(dst.data(), src.data());
}

/// Screen position of the top vertex of tile (x, y).
std::pair<int, int> tile_top(int x, int y, std::size_t group);

/// Pixels per HEIGHT unit at zoom group 0, as a fraction num / den.
struct Lift {
    int num = 0, den = 1;
    /// Screen offset (upwards) of a tile with height `h` at `group`.
    [[nodiscard]] int pixels(std::uint8_t h, std::size_t group) const {
        return scaled(h, 1, group);
    }
    /// Offset for the height h_num / h_den (integer division toward zero).
    [[nodiscard]] int scaled(std::int64_t h_num, std::int64_t h_den, std::size_t group) const;
    bool operator==(const Lift&) const = default;
};
inline constexpr Lift kNoLift{0, 1};
/// The original's scale: one HEIGHT unit is 0.04 world units (D:
/// AnnoWelt.dll height table 0x100E35E0) and a world unit of height is
/// 51.477 px at zoom 1.0 (D: AnnoFrame.dll camera 0x10071d70: scale
/// 59.1633, tilt 29.53°), so ≈ 2.059 px.
inline constexpr Lift kHeightLift{2059, 1000};

/// Corner heights of tile (x, y): [(x, y), (x+1, y), (x, y+1), (x+1, y+1)],
/// clamped at the island edge.
std::array<std::int64_t, 4> corners(std::span<const std::uint8_t> heights, std::size_t w,
                                    std::size_t h, std::size_t x, std::size_t y);

/// Integer height at tile position (i/n, j/n), times n (the test oracle of
/// terrain_height): triangles A-B-C for i + j ≤ n, C-B-D beyond (D:
/// AnnoWelt.dll mesh builder 0x1009b920).
std::int64_t interpolate(std::array<std::int64_t, 4> c, std::int64_t i, std::int64_t j,
                         std::int64_t n);

/// Terrain height (HEIGHT units) at tile position (x, y), over the tile's
/// two triangles as the original (D: AnnoWelt.dll 0x1009b920).
float terrain_height(std::span<const std::uint8_t> heights, std::size_t w, std::size_t h, float x,
                     float y);

/// Where the ground drawn at each canvas pixel lies: the tile position of the
/// visible terrain point, NaN where no ground was drawn.
struct GroundProbe {
    std::vector<std::array<float, 2>> pos;
    static GroundProbe for_canvas(const Canvas& canvas);
};

/// Draws every ground tile that can touch the canvas, back to front by
/// (x + y, x); records the tile position of each drawn pixel in `probe`.
void draw_ground_grids(Canvas& canvas, std::size_t w, std::size_t h,
                       std::span<const std::uint16_t> gfx, std::span<const std::uint8_t> heights,
                       const assets::island::GroundTextures& textures, std::size_t group, Lift lift,
                       GroundProbe* probe = nullptr);

/// draw_ground_grids from an island's own grids (nothing if unreadable).
void draw_ground(Canvas& canvas, const formats::insel::Island& island,
                 const assets::island::GroundTextures& textures, std::size_t group, Lift lift);

/// Outcome of drawing objects.
struct DrawStats {
    std::size_t drawn = 0;
    /// Objects whose ANLAGEN entry has no sprite set (invisible markers).
    std::size_t invisible = 0;
    /// Objects that should be drawable but are not.
    std::size_t failed = 0;
    void add(const DrawStats& o) {
        drawn += o.drawn;
        invisible += o.invisible;
        failed += o.failed;
    }
    bool operator==(const DrawStats&) const = default;
};

/// ANLAGEN entry of the open-sea surface (MEER, 64 BSH_RGBWZ frames; D:
/// AnnoWelt.dll 0x1009f4c0). Original value; to become content
/// configuration (PLAN §0.1).
inline constexpr std::uint32_t kSeaAnlage = 2801;
/// Sea level in HEIGHT units: 1.28 world units / 0.04 (D: AnnoWelt.dll
/// 0x100E3A20, set at 0x1009d010).
inline constexpr std::int64_t kSeaLevel = 32;
/// Waterline in HEIGHT units: terrain below it is under water (P: painted
/// ground textures put the shore at 16.5; D: land classifier 0x100900c0;
/// docs/render/iso.md, "Water"; OQ-026 b).
inline constexpr std::int64_t kWaterline = 16;
/// A tile counts as land when the average of its corner heights exceeds this
/// (D: AnnoWelt.dll classifier 0x100900c0).
inline constexpr std::int64_t kLandAverage = 19;
/// Display kind of water objects: byte 2 of the sprite-set prefix (D:
/// AnnoDisplay.dll factory 0x10025d30; tHausAnimWasser).
inline constexpr std::uint8_t kKindWater = 10;
/// ANLAGEN entry of surf (Brandung01), whose phase the original sets to
/// (3y + 5x) mod 32 on load (D: AnnoWelt.dll 0x1007aa40, 0x1007ab58).
inline constexpr std::uint32_t kSurfAnlage = 2708;

/// Which placed objects to draw.
enum class Objects {
    All,
    /// Everything except water objects (static layer).
    NotWater,
    /// Water objects below the waterline (surf on the sea floor).
    WaterBelowSea,
    /// Water objects at or above it (rivers).
    WaterAboveSea,
};

/// (ANLAGEN id, variant, shadow, zoom group, frame index); variant 0 = the
/// frame as decoded, kSmoothed = its 2 × 2 smoothed copy.
using SpriteKey = std::tuple<std::uint32_t, std::size_t, bool, std::uint32_t, std::size_t>;
/// SpriteKey variant: the frame averaged over 2 × 2 pixels (enhanced water,
/// not original).
inline constexpr std::size_t kSmoothed = 1;

/// `img` averaged over each pixel's 2 × 2 neighbourhood (right and down,
/// premultiplied alpha; outside counts as transparent).
Rgba smooth_2x2(const Rgba& img);

/// Decoded frames shared between renders and threads.
class SpriteCache {
  public:
    /// The decoded frame `key` (`frame` = its header), decoded on first use;
    /// nullptr if it cannot be decoded.
    std::shared_ptr<const Rgba> get(const formats::anlagen::AnlagenFile& anlagen,
                                    const SpriteKey& key, const formats::figuren::Frame& frame);

  private:
    std::shared_mutex mutex_;
    std::map<SpriteKey, std::shared_ptr<const Rgba>> frames_;
};

/// Animation of a sprite set from its ANIM record: bytes 2–3 = frames per
/// animation (≥ 1), bytes 4–5 = milliseconds per frame (0 = static).
std::pair<std::size_t, std::uint64_t> animation(const formats::figuren::GfxSet& set);

/// Frame index: rotation block rotation · anim, then the animation step
/// (time / ms + phase) mod anim (no animation when ms is 0).
std::size_t frame_index(std::size_t total, std::pair<std::size_t, std::uint64_t> anim,
                        std::uint8_t rotation, std::uint64_t time_ms, std::size_t phase);

/// Draws `img` with its anchor at screen point (sx, sy); `pixel(x, y, rgba)`
/// decides per pixel whether (and what) to draw.
template <class F>
void blit_sprite(Canvas& canvas, const Rgba& img, const formats::figuren::Frame& frame,
                 std::pair<int, int> at, bool shadow, F&& pixel) {
    const int ox = at.first - frame.anchor_x;
    const int oy = at.second - frame.anchor_y;
    if (!canvas.overlaps(ox, oy, img.width, img.height)) {
        return;
    }
    const std::size_t w = img.width;
    const std::size_t n = img.pixels.size() / 4;
    // Lowest opaque row per column, for Canvas::base (objects, not shadows).
    const bool track = !shadow && !canvas.base.empty();
    std::vector<std::size_t> keel;
    if (track) {
        keel.assign(w, 0);
        for (std::size_t i = 0; i < n; ++i) {
            if (img.pixels[i * 4 + 3] != 0) {
                keel[i % w] = i / w;
            }
        }
    }
    auto contact = [&](std::size_t col) {
        return static_cast<int>(keel[col]) + canvas.base_band >= frame.anchor_y;
    };
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint8_t* px = &img.pixels[i * 4];
        if (px[3] == 0) {
            continue;
        }
        const int x = ox + static_cast<int>(i % w);
        const int y = oy + static_cast<int>(i / w);
        const std::optional<Px> rgba = pixel(x, y, Px{px[0], px[1], px[2], px[3]});
        if (!rgba) {
            continue;
        }
        canvas.put(x, y, shadow ? kShadow : *rgba);
        if (track) {
            if (const auto o = canvas.index(x, y)) {
                canvas.base[*o / 4] =
                    contact(i % w)
                        ? static_cast<std::uint8_t>(std::min<std::size_t>(keel[i % w] - i / w, 254))
                        : 255;
            }
        }
    }
}

/// A placed object with its footprint (w, h), ready to draw.
using Prepared = std::pair<formats::insel::PlacedObject, std::pair<std::uint8_t, std::uint8_t>>;

/// The placed objects of `records` selected by `which`, duplicates removed,
/// in draw order (by x + w + y + h, then x; stable). Objects whose entry is
/// missing or has no sprite set are kept (counted when drawn).
std::vector<Prepared> prepare_objects(const formats::insel::Island& island,
                                      const formats::anlagen::AnlagenFile& anlagen,
                                      std::span<const std::string_view> records, Objects which);

/// One sprite to draw: its frame header, the key of its decoded image and the
/// screen point its anchor lands on.
struct SpriteDraw {
    SpriteKey key;
    const formats::figuren::Frame* frame = nullptr;
    std::pair<int, int> anchor{0, 0};
    /// The object's height byte (HEIGHT units).
    std::uint8_t height = 0;

    /// Screen rectangle (x, y, w, h) of the frame box.
    [[nodiscard]] std::array<int, 4> rect() const;
    /// rect() widened by kOverrunSlack to the right (for culling).
    [[nodiscard]] std::array<int, 4> reach() const;
    [[nodiscard]] bool is_shadow() const { return std::get<2>(key); }
};

/// Columns a frame's rows may write right of its frame box: the original
/// draws them (OQ-025). P: the largest overrun in Anlagen.dat at any zoom
/// group (crates/render/tests/sprite_fidelity.rs); data with larger overruns
/// needs this raised or computed per scene.
inline constexpr int kOverrunSlack = 16;

/// The sprites of prepared objects at time_ms, in draw order (each object's
/// shadow before it), keeping those whose reach() passes `visible`. Each
/// object stands at its own height with its own phase (surf: (3y + 5x) mod
/// 32).
template <class V>
DrawStats place_prepared(std::span<const Prepared> list,
                         const formats::anlagen::AnlagenFile& anlagen, std::size_t group, Lift lift,
                         std::uint64_t time_ms, V&& visible, std::vector<SpriteDraw>& out);

/// Draws prepared objects at time_ms, a sprite pixel at canvas point (x, y)
/// only where keep(x, y, object height) holds (a depth test); non-shadow
/// sprites use `variant`.
template <class K>
DrawStats draw_prepared_tested(Canvas& canvas, std::span<const Prepared> list,
                               const formats::anlagen::AnlagenFile& anlagen, std::size_t group,
                               Lift lift, std::uint64_t time_ms, SpriteCache& cache,
                               std::size_t variant, K&& keep);

/// draw_prepared_tested with variant 0 and no depth test.
DrawStats draw_prepared(Canvas& canvas, std::span<const Prepared> list,
                        const formats::anlagen::AnlagenFile& anlagen, std::size_t group, Lift lift,
                        std::uint64_t time_ms, SpriteCache& cache);

/// prepare_objects then draw_prepared.
DrawStats draw_objects(Canvas& canvas, const formats::insel::Island& island,
                       const formats::anlagen::AnlagenFile& anlagen,
                       std::span<const std::string_view> records, Objects which, std::size_t group,
                       Lift lift, std::uint64_t time_ms, SpriteCache& cache);

/// Average colour of the opaque pixels of the sea sprite's first frame
/// (group 0), for open-water ground (approximation, OQ-026 b).
std::optional<Px> water_colour(const formats::anlagen::AnlagenFile& anlagen);

/// How the sea is drawn.
enum class WaterStyle {
    /// As the original (default).
    Original,
    /// Not in the original (opt-in): swell, breaking waves, smooth shore.
    Enhanced,
};

/// Distance in tiles from each height corner to the nearest corner at or
/// above the waterline (two-pass chamfer, 1 / √2 steps). Enhanced water.
std::vector<float> shore_distance(std::span<const std::uint8_t> heights, std::size_t w,
                                  std::size_t h);

/// Tuning of the enhanced water. Not derived from the original; tuned by eye
/// (2026-10-17). gpu.wgsl mirrors these values.
struct Waves {
    /// Tiles from the shore where coastal waves start.
    float surf_zone;
    /// Water depth (HEIGHT units below the waterline) where waves start to
    /// grow, where they break, and below which swash runs up the beach.
    float shoal_depth, break_depth, swash_depth;
    /// Direction the open-sea swell travels in (tile axes, unit length).
    std::array<float, 2> swell_dir;
    /// Distance between surf crests near the shore (tiles).
    float surf_wavelength;
    /// Largest surf wave (HEIGHT units), breaker index γ, gain on Hunt's
    /// run-up, largest horizontal run-up (tiles).
    float wave_height, breaker_index, runup_gain, runup_max_tiles;
    /// Depth over which shallow water is tinted.
    float tint_depth;
    /// Distance between incoming crests (tiles) and time between them (ms).
    float wavelength, period_ms;
    /// Brightness amplitude of the open-water swell; strength and colour
    /// (0–1 RGB) of the shallow-water tint.
    float swell, shallow_tint;
    std::array<float, 3> shallow_colour;
    /// Depth where the sea floor starts to fade out and where it is hidden.
    float deep_from, deep_to;
    /// Amplitude of the noise on the enhanced waterline and half width of
    /// its soft edge (HEIGHT units).
    float shore_noise, shore_edge;
};
inline constexpr Waves kWaves = {
    .surf_zone = 4.0F,
    .shoal_depth = 12.0F,
    .break_depth = 4.0F,
    .swash_depth = 1.5F,
    // Crests run diagonally on screen (crest along the tile y axis).
    .swell_dir = {0.96F, 0.28F},
    .surf_wavelength = 1.8F,
    .wave_height = 6.0F,
    .breaker_index = 0.78F,
    .runup_gain = 1.6F,
    .runup_max_tiles = 2.0F,
    .tint_depth = 8.0F,
    .wavelength = 2.6F,
    .period_ms = 3400.0F,
    .swell = 0.07F,
    .shallow_tint = 0.3F,
    .shallow_colour = {0.36F, 0.74F, 0.76F},
    .deep_from = 4.0F,
    .deep_to = 14.0F,
    .shore_noise = 2.0F,
    .shore_edge = 0.35F,
};

/// 0 below a, 1 above b, smooth in between (also for a > b).
float smoothstep(float a, float b, float x);
/// Smooth value noise in [0, 1) (trilinear, smoothstep fade).
float noise3(float x, float y, float z);
/// How much of the sea floor deep water hides (0–1) at `depth`.
float deep_water(float depth);
/// Detail level of the enhanced water for tile width `tw` (pixels).
float wave_detail(float tw);
/// Relative height (≈ 0.15–1) of surf wave number k near pos.
float wave_height(float k, std::pair<float, float> pos);
/// Vertical run-up (HEIGHT units) of a wave of relative height a on a beach
/// of `slope` (Hunt 1959, R = ξ · H).
float runup_height(float a, float slope);

/// One HEIGHT unit in tiles (docs/render/iso.md, "Elevation": 0.04 world units).
inline constexpr float kHeightUnitTiles = 0.04F;

/// (light, foam, tint, aeration) of an enhanced-water pixel. Aeration is the
/// mix toward kAerationColour of rapids (rivers.md 2.8, D5: never white, no
/// foam on rivers); only the river flow shading sets it.
struct Shade {
    float light = 0, foam = 0, tint = 0, aeration = 0;
};
/// The colour (0-1 per channel) that aerated river water turns toward: a light
/// blue-green, between the shallow tint and the foam white (rivers.md 2.8).
/// Tuning value.
inline constexpr std::array<float, 3> kAerationColour = {0.62F, 0.82F, 0.90F};
Shade wave_shade(float dist, float depth, float beach_slope, std::pair<float, float> pos,
                 std::uint64_t time_ms, float detail);
/// Shore distance (tiles) of the sea look blended into rivers at the coast:
/// calm water without surf. Tuning value (not original).
inline constexpr float kRiverSeaDist = 3.0F;
/// River-surface slope (HEIGHT units per tile) where rapids start and where
/// they are full whitewater. Tuning values (not original).
inline constexpr float kRapidsFrom = 5.0F;
inline constexpr float kRapidsFull = 14.0F;
/// Enhanced river water: a slow shimmer, faster and stronger where the river
/// surface falls by more than kRapidsFrom HEIGHT units per tile; no foam.
Shade river_shade(std::pair<float, float> pos, float depth, float slope, std::uint64_t time_ms);
/// (run-up in HEIGHT units, time since the wave reached the beach as a
/// fraction of a period, relative wave height).
std::array<float, 3> swash(float dist, float slope, std::pair<float, float> pos,
                           std::uint64_t time_ms);
float swash_runup(float dist, float slope, std::pair<float, float> pos, std::uint64_t time_ms);
/// How enhanced rivers are shaded (docs/render/rivers.md; --river-flow, key R):
/// the legacy shimmer that slides along fixed axes, or the flow-map shading
/// advected along the solved flow (stage S5, CPU reference path).
enum class RiverFlowMode { Legacy, Flow };

/// Cost tiers of the river flow shading (docs/render/rivers.md 4 and 16, decision
/// D11). High: every layer, the ripple light as the slope of the noise (two
/// taps per phase). Medium: the layers that fade with the zoom group's wave
/// detail (the glints and the whitewater streaks) are left out. Low: Medium
/// with one noise tap per phase (the noise itself, scaled to the light
/// variance of the slope, instead of its slope). The numbers are the
/// enumerators of the uniform's `quality` field (gpu.wgsl).
enum class RiverQuality : std::uint32_t { Low = 0, Medium = 1, High = 2 };

/// RiverQuality::Low: the light of a ripple layer is amp * kLowTapGain * scale *
/// noise instead of amp * (noise(x) - noise(x - e)) / e: the slope of the
/// value noise at scale s has the standard deviation of the noise times about
/// this factor times s (calibrated by the test "river quality tiers": the
/// light variance of Low stays within 25 % of High on a uniform flow).
/// Tuning value (not evidence).
inline constexpr float kLowTapGain = 1.8F;

/// Layers of the river flow shading in RiverParams. The count is data
/// (RiverConfig::layers); the array size is the compile-time bound of the
/// uniform array of the GPU port (rivers.md 3.3: a WGSL uniform array needs a
/// size; raising it is this constant plus the shader override).
inline constexpr std::size_t kMaxRiverLayers = 8;

/// One layer of the river flow shading, the twin of WGSL `RiverLayer`.
struct RiverLayer {
    /// Noise frequency (1/tile), period of the flow-map phase (ms; must divide
    /// 3,600,000 so that the hourly time wrap is seamless), light amplitude.
    /// (The design's stretch along the flow is not built: applied to the
    /// island's tile coordinates it moves the noise coordinate by up to ten
    /// times the geometry wherever the flow varies, i.e. aliases;
    /// docs/render/rivers.md 15.7.)
    float scale = 1.0F, period_ms = 1000.0F, amp = 0.0F;
    /// 1: the amplitude follows the zoom group's wave detail (wave_detail).
    float fades = 0.0F;
    /// 1: a rapids layer: no emboss, the light is rapids(slope) * (rapids_base
    /// + amp * 2 * (noise - 1/2)), as the legacy shimmer's fast layers.
    float rapids = 0.0F;
};

/// The layer table and the scalars of the flow shading (the uniform buffer
/// `river_params` of the GPU port).
struct RiverParams {
    std::uint32_t count = 0;
    /// The cost tier (RiverQuality; High = the reference look).
    RiverQuality quality = RiverQuality::High;
    /// Constant brightening of a full rapids layer; the shear fade of
    /// layers (T |grad u| from `shear_from` to `shear_to`).
    float rapids_base = 0.05F, shear_from = 0.5F, shear_to = 1.5F;
    /// Stage S7 (rivers.md 2.5-2.8; RiverConfig has the meaning of each): the
    /// Froude number is |u| / sqrt(gravity * 0.04 * max(depth, depth_min))
    /// from the baked velocity and the pixel's water depth (HEIGHT units); the
    /// rapids factor is max(slope rule, smoothstep(froude_from, froude_to, Fr));
    /// aeration, wake and standing waves follow.
    float froude_from = 0.55F, froude_to = 0.95F, gravity = 14.0625F, depth_min = 1.5F;
    float aeration = 0.35F, wake_dark = 0.06F, wake_boost = 0.6F;
    /// Standing waves: off (0) or on (1), amplitude, Froude range, wavelength
    /// range (tiles).
    float standing = 0.0F, standing_amp = 0.05F, standing_from = 0.5F, standing_to = 1.0F;
    float standing_min = 0.45F, standing_max = 2.5F;
    std::array<RiverLayer, kMaxRiverLayers> layers{};
};

/// The river flow at a pixel, from the baked lattice (river::flow_at): the
/// velocity (tiles / s along the tile axes), the turbulence and wake channels
/// (0 until the hydraulic extras), the river surface level (HEIGHT units),
/// the weight of valid texels (0 = no flow here: legacy shimmer) and the
/// shear |grad u| (1/s).
struct FlowSample {
    float vx = 0, vy = 0, turb = 0, wake = 0, level = 0, valid = 0, shear = 0;
};

/// Per zoom group constants of the flow shading: tile size (pixels) and
/// pixels per HEIGHT unit of the lift (E19: the pattern lives on the river's
/// own water plane).
struct RiverView {
    float tw = 84.0F, th = 42.0F, lambda = 2.059F;
};

/// Phases of one layer at a point (rivers.md 2.8, E17): the two noise
/// coordinates of the flow-map pair and their weights (sum 1).
struct FlowPhases {
    std::array<std::array<float, 2>, 2> c{};
    std::array<float, 2> w{};
};
/// The flow-map phases of `layer` at water-plane position `x` (tiles) moving
/// at velocity `u` (tiles / s), at `time_ms`. c[k] = (x - u T phi_k) s + o_k,
/// d c_k / dt = -u s: a feature moves downstream with exactly u, and c_k
/// changes by s per tile of x plus s T phi_k per tile / s of u.
FlowPhases flow_phases(std::array<float, 2> x, std::array<float, 2> u, std::uint64_t time_ms,
                       const RiverLayer& layer, float psi);
/// The slow, position-dependent phase offset psi(x) = 1/2 noise(0.13 x) that
/// keeps neighbouring places out of step (no global pulsing, E17).
float flow_psi(std::array<float, 2> x);

/// (light, 0, tint) of an enhanced river pixel by flow-map advection of the
/// ripple layers along the solved velocity (stage S5; twin of the WGSL
/// river_flow_shade of the GPU port). (px, py) are the scene pixel's
/// coordinates as river_shade's callers use them (before the water-plane
/// lift); `depth` and `slope` as for river_shade; `detail` is wave_detail.
Shade river_flow_shade(float px, float py, const FlowSample& f, float depth, float slope,
                       std::uint64_t time_ms, float detail, const RiverParams& params,
                       const RiverView& view);

/// Period (ms; divides 3,600,000, so the hourly wrap is seamless) of the flow map
/// that carries the waterline ripple of objects standing in a river.
inline constexpr float kObjectRipplePeriodMs = 1600.0F;
/// The ripple (pixels) of the waterline of an object standing in a river
/// (water against objects, docs/render/iso.md; twin of WGSL
/// `object_river_ripple`): two layers of value noise (a cell of 10 and of 3.8
/// pixels at zoom group 0, the sizes of the legacy ripple's two sines) carried
/// by the two-phase flow map along the water's screen velocity (vpx, vpy) in
/// pixels / s, so a crest drifts with the river exactly; the weights of the two
/// phases blend variance-preserving as in river_flow_shade. (fx, fy) is the
/// scene pixel of the object's column and of its foot, `s` the zoom group's
/// scale (tile height / 42). The gains make its standard deviation that of the
/// legacy ripple, 0.334 s pixels (0.4 and 0.25 times a sine).
inline constexpr float kObjectRippleFreq1 = 0.1F, kObjectRippleFreq2 = 0.26F;
inline constexpr float kObjectRippleGain1 = 1.9F, kObjectRippleGain2 = 1.2F;
float object_river_ripple(float fx, float fy, float s, std::uint64_t time_ms, float vpx, float vpy);

/// `rgba` tinted, lit and whitened by `s`.
Px shade_apply(Px rgba, Shade s);

// --- template definitions ---

template <class V>
DrawStats place_prepared(std::span<const Prepared> list,
                         const formats::anlagen::AnlagenFile& anlagen, std::size_t group, Lift lift,
                         std::uint64_t time_ms, V&& visible, std::vector<SpriteDraw>& out) {
    DrawStats stats;
    const auto g = static_cast<std::uint32_t>(group);
    const auto [tw, th] = kTile[group];
    for (const auto& [o, fp] : list) {
        const auto [w, h] = fp;
        const std::uint32_t id = o.anlage;
        const formats::figuren::Entry* entry = anlagen.entry(id);
        if (entry == nullptr) {
            ++stats.failed;
            continue;
        }
        if (entry->gfx.empty()) {
            ++stats.invisible;
            continue;
        }
        const formats::figuren::GfxSet& set = entry->gfx.front();
        // Footprint centre in doubled units.
        const int cx2 = 2 * int{o.x} + int{w};
        const int cy2 = 2 * int{o.y} + int{h};
        const std::int64_t z2 = 2 * std::int64_t{o.height()};
        const int raise = lift.scaled(z2, 2, group);
        const std::pair<int, int> anchor{(cx2 - cy2) * tw / 4, (cx2 + cy2) * th / 4 - raise};
        const formats::figuren::FrameSet* frames = nullptr;
        for (const auto& fs : set.frame_sets) {
            if (fs.group == g) {
                frames = &fs;
                break;
            }
        }
        if (frames == nullptr) {
            ++stats.failed;
            continue;
        }
        const std::size_t phase = id == kSurfAnlage
                                      ? (3 * std::size_t{o.y} + 5 * std::size_t{o.x}) % 32
                                      : std::size_t{o.phase()};
        const std::size_t k =
            frame_index(frames->frames.size(), animation(set), o.rotation, time_ms, phase);
        if (k >= frames->frames.size()) {
            ++stats.failed;
            continue;
        }
        ++stats.drawn;
        if (set.shadow) {
            for (const auto& fs : set.shadow->frame_sets) {
                if (fs.group == g) {
                    if (k < fs.frames.size()) {
                        const SpriteDraw d{{id, 0, true, g, k}, &fs.frames[k], anchor, o.height()};
                        const auto r = d.reach();
                        if (visible(r[0], r[1], r[2], r[3])) {
                            out.push_back(d);
                        }
                    }
                    break;
                }
            }
        }
        const SpriteDraw d{{id, 0, false, g, k}, &frames->frames[k], anchor, o.height()};
        const auto r = d.reach();
        if (visible(r[0], r[1], r[2], r[3])) {
            out.push_back(d);
        }
    }
    return stats;
}

template <class K>
DrawStats draw_prepared_tested(Canvas& canvas, std::span<const Prepared> list,
                               const formats::anlagen::AnlagenFile& anlagen, std::size_t group,
                               Lift lift, std::uint64_t time_ms, SpriteCache& cache,
                               std::size_t variant, K&& keep) {
    std::vector<SpriteDraw> draws;
    DrawStats stats = place_prepared(
        list, anlagen, group, lift, time_ms,
        [&canvas](int x, int y, int w, int h) { return canvas.overlaps(x, y, w, h); }, draws);
    for (auto& d : draws) {
        if (!d.is_shadow()) {
            std::get<1>(d.key) = variant;
        }
    }
    for (const auto& d : draws) {
        const auto img = cache.get(anlagen, d.key, *d.frame);
        if (!img) {
            if (!d.is_shadow()) {
                --stats.drawn;
                ++stats.failed;
            }
            continue;
        }
        const std::uint8_t h = d.height;
        blit_sprite(canvas, *img, *d.frame, d.anchor, d.is_shadow(),
                    [&keep, h](int x, int y, Px p) -> std::optional<Px> {
                        if (keep(x, y, h)) {
                            return p;
                        }
                        return std::nullopt;
                    });
    }
    return stats;
}

} // namespace anno::render
