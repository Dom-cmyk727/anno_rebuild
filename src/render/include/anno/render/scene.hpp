// Scenario composition: every island of a scenario at its world position,
// each rendered with iso.hpp and composited into one screen region.
//
// Rules (spec: docs/render/iso.md, "Scene composition"):
// - Island tile (0, 0) is world tile (Island::x, Island::y) (INSEL5 head);
//   an island's local screen coordinates are shifted by tile_top(x, y).
// - An island covers the rectangle of its tile diamonds plus margin(group)
//   above the top vertex for sprites that rise above the top row.
// - Islands are drawn by their far corner x + w + y + h, then x (OQ-026).
// - An island is rendered into a temporary canvas covering only its overlap
//   with the target region, then composited source-over.
// - Ships float on the sea surface (kSeaLevel) and are drawn after all
//   islands, back to front by x + y. Soldiers stand on the ground and are
//   drawn with their island's objects as a 1 × 1 footprint on their tile.
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <anno/assets/island.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/rect.hpp>
#include <anno/render/sea.hpp>

namespace anno::render {

/// Room above an island's top vertex per zoom group: the largest sprite
/// anchor height in Anlagen.dat (P: entry 1330, anchor_y 504 / 252 / 126).
inline constexpr std::array<int, kGroups> kMargin = {504, 252, 126};

/// kMargin plus the largest elevation (255 at kHeightLift).
int margin(std::size_t group);

/// World position and size of an island in tiles (INSEL5 head).
struct IslandBox {
    std::uint16_t x = 0, y = 0, width = 0, height = 0;
    static IslandBox of(const formats::insel::Island& island) {
        return {island.x, island.y, island.width, island.height};
    }
};

/// Where an island lands on the scene screen at one zoom group.
struct Placement {
    /// Index into the island list given to layout().
    std::size_t index = 0;
    /// Scene screen position of the island's local point (0, 0).
    std::pair<int, int> offset{0, 0};
    /// Scene screen rectangle the island can draw into.
    Rect bounds;
    bool operator==(const Placement&) const = default;
};

/// Placements of all islands at `group`, in draw order (back to front).
std::vector<Placement> layout(std::span<const IslandBox> boxes, std::size_t group);
/// Union of all placement bounds; nullopt without islands.
std::optional<Rect> scene_bounds(std::span<const Placement> placements);
/// A transparent canvas covering `rect`.
Canvas blank(Rect rect);
/// Composites src source-over onto dst; src's point (sx, sy) lands on dst's
/// point (sx + offset.first, sy + offset.second).
void composite(Canvas& dst, const Canvas& src, std::pair<int, int> offset);

/// Outcome of Scene::render.
struct SceneStats {
    std::size_t islands = 0;
    /// Of those, islands whose grids could not be read.
    std::size_t no_ground = 0;
    DrawStats objects;
    std::size_t units = 0;
};

/// Layers Scene::render draws, in this order: ground; water objects below
/// the waterline; the sea surface; water objects above it; other objects.
struct Layers {
    bool ground = false, water_below = false, sea = false, water_above = false, objects = false;
    std::uint64_t time_ms = 0;
    WaterStyle water_style = WaterStyle::Original;
    /// With `objects`: also the ships (units on the sea surface, drawn after
    /// all island objects). The GPU compositor draws them itself instead.
    bool ships = true;

    static constexpr Layers all() {
        return {true, true, true, true, true, 0, WaterStyle::Original};
    }
    static constexpr Layers none() { return {}; }
};

/// A layer of placed-object sprites (Scene::sprites).
enum class SpriteLayer { WaterBelow, WaterAbove, Objects };

/// ANLAGEN ids of river pieces (Fluss*; P: docs/render/iso.md, "Water").
/// Original values; to become content configuration (PLAN §0.1).
inline constexpr std::array<std::uint32_t, 4> kRiverAnlagen = {2701, 2705, 2706, 2707};
/// Tolerance (HEIGHT units) of the river depth test. Tuning value (OQ-026 b).
inline constexpr float kRiverTolerance = 10.0F;
/// Object records drawn: scenery, built objects, rivers and surf.
inline constexpr std::array<std::string_view, 3> kObjectRecords = {"GELAENDE", "PROD2", "FLUESSE"};

/// Average colour of the most common sea floor texture at the lowest HEIGHT
/// of the islands' textured tiles (ties: lowest id), for open water.
std::optional<Px> sea_floor_colour(std::span<const formats::insel::Island> islands,
                                   const assets::island::GroundTextures& textures);

/// A decoded frame and its image (a null image is skipped when drawing).
struct UnitFrame {
    formats::figuren::Frame frame;
    std::shared_ptr<const Rgba> image;
};

/// A unit to draw: world position in tiles, the ground height it stands on
/// (nullopt = on the sea surface), its frame and shadow per zoom group.
struct UnitSprite {
    float x = 0, y = 0;
    std::optional<float> z;
    std::vector<std::optional<UnitFrame>> frames;
    std::vector<std::optional<UnitFrame>> shadows;

    /// Scene screen point of the frame anchor at `group`.
    [[nodiscard]] std::pair<int, int> anchor(std::size_t group, Lift lift) const;
    /// Draws shadow and frame with the anchor shifted by −offset; whether
    /// the frame touched the canvas.
    bool draw(Canvas& canvas, std::size_t group, Lift lift, std::pair<int, int> offset) const;
};

/// Everything needed to draw a scenario. Islands and Anlagen.dat refer to
/// the caller's file buffers, which must outlive the scene. Every `group`
/// argument must be < kGroups (callers validate user input).
class Scene {
  public:
    /// Open water gets sea_floor_colour() if there is one, else the colour
    /// already set in `textures`.
    Scene(std::string name, std::vector<formats::insel::Island> islands,
          assets::island::GroundTextures textures, formats::anlagen::AnlagenFile anlagen,
          Lift lift);
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    std::string name;
    std::vector<formats::insel::Island> islands;
    assets::island::GroundTextures textures;
    formats::anlagen::AnlagenFile anlagen;
    /// Placements per zoom group, in draw order.
    std::array<std::vector<Placement>, kGroups> layouts;
    /// Decoded sprite frames, shared by all renders and threads.
    mutable SpriteCache sprites;
    Lift lift;
    /// Ships and soldiers (empty unless set by the caller).
    std::vector<UnitSprite> units;

    /// The sea pattern of `group`, rendered on first use (thread-safe).
    const sea::SeaPattern* sea(std::size_t group) const;
    /// The sea pattern of `group` if it has been rendered already (never
    /// blocks).
    [[nodiscard]] const sea::SeaPattern* sea_if_ready(std::size_t group) const;
    /// Whether island `index` has water objects below / above the waterline.
    [[nodiscard]] std::pair<bool, bool> has_water_objects(std::size_t index) const;
    /// The sprites of `layer` touching `rect` at time_ms, in draw order,
    /// anchors in scene coordinates.
    [[nodiscard]] std::vector<SpriteDraw> sprites_in(Rect rect, std::size_t group,
                                                     std::uint64_t time_ms, SpriteLayer layer,
                                                     WaterStyle style) const;
    /// Static sea data of `rect`.
    [[nodiscard]] sea::WaterField water_field(Rect rect, std::size_t group) const;
    [[nodiscard]] sea::WaterField water_field_from(Rect rect, const sea::SceneProbe& probe) const;
    /// Water depth for the wave simulation at world tile (x, y).
    [[nodiscard]] float fluid_depth(float x, float y) const;
    /// Terrain height seen at scene pixel (sx, sy) of `rect`.
    [[nodiscard]] std::optional<float> terrain_at(const sea::SceneProbe& probe, Rect rect, int sx,
                                                  int sy) const;
    /// The ground of `rect` and, per pixel, which island's terrain is seen.
    [[nodiscard]] std::pair<Canvas, sea::SceneProbe> ground_with_probe(Rect rect,
                                                                       std::size_t group) const;
    /// Renders `layers` of `rect` at `group` into a new canvas.
    [[nodiscard]] std::pair<Canvas, SceneStats> render(Rect rect, std::size_t group,
                                                       Layers layers) const;
    /// render() with the terrain height at each scene pixel given by
    /// `terrain` (from a chunk cache) for the river depth test.
    [[nodiscard]] std::pair<Canvas, SceneStats>
    render_with_terrain(Rect rect, std::size_t group, Layers layers,
                        const std::function<std::optional<float>(int, int)>* terrain) const;
    /// Ground height at world point (x, y); nullopt over open sea.
    [[nodiscard]] std::optional<float> ground_height(float x, float y) const;
    /// Whether any unit is drawn into `rect` at `group`.
    /// With `ships` false, units on the sea surface are not counted.
    [[nodiscard]] bool units_touch(Rect rect, std::size_t group, bool ships = true) const;
    /// Scene rectangle covering all islands and units.
    [[nodiscard]] std::optional<Rect> bounds(std::size_t group) const;

  private:
    struct IslandData {
        std::vector<Prepared> below, below_enhanced, above, above_enhanced, other;
        std::vector<sea::RiverPiece> rivers;
        /// nullopt if a grid cannot be read (no ground drawn).
        std::optional<std::pair<std::vector<std::uint16_t>, std::vector<std::uint8_t>>> grids;
        std::unique_ptr<std::once_flag> water_once = std::make_unique<std::once_flag>();
        std::optional<sea::IslandWater> water;

        [[nodiscard]] const std::vector<Prepared>& below_list(WaterStyle s) const {
            return s == WaterStyle::Original ? below : below_enhanced;
        }
        [[nodiscard]] const std::vector<Prepared>& above_list(WaterStyle s) const {
            return s == WaterStyle::Original ? above : above_enhanced;
        }
    };
    struct SeaSlot {
        std::once_flag once;
        std::optional<sea::SeaPattern> pattern;
        /// Set (release) once `pattern` is final; read by sea_if_ready.
        std::atomic<bool> ready{false};
    };

    const sea::IslandWater* island_water(std::size_t i) const;
    std::pair<DrawStats, std::size_t> draw_island_objects(Canvas& local, const Placement& p,
                                                          std::span<const Prepared> list,
                                                          std::size_t group) const;
    [[nodiscard]] std::optional<Rect> unit_rect(const UnitSprite& u, std::size_t group) const;
    std::size_t draw_units(Canvas& canvas, std::size_t group) const;

    mutable std::vector<IslandData> objects_;
    mutable std::array<SeaSlot, kGroups> seas_;
};

} // namespace anno::render
