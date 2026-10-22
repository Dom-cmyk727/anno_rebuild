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
#include <deque>
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
#include <anno/assets/unit.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/chunk_draws.hpp>
#include <anno/render/frame_table.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/rect.hpp>
#include <anno/render/river.hpp>
#include <anno/render/sea.hpp>
#include <anno/render/zoom_levels.hpp>

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

/// One island's height grid as terrain_plane reads it (heights nullptr: the
/// island has no ground).
struct TerrainGrid {
    const std::vector<std::uint8_t>* heights = nullptr;
    std::size_t w = 0, h = 0;
};
/// Chunk::terrain for the pixels of `probe`: terrain_height at each pixel's
/// position on its island (grid(i)), in 1/kTerrainScale HEIGHT units,
/// kNoTerrain where no ground was drawn.
std::vector<std::uint16_t> terrain_plane(const sea::SceneProbe& probe,
                                         const std::function<TerrainGrid(std::uint32_t)>& grid);
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
    /// Track Canvas::base (object pixels' height above their lowest row).
    bool bases = false;
    /// Enhanced water: how rivers are shaded (the flow shading is the
    /// default; Legacy keeps the shimmer that slides along fixed axes).
    RiverFlowMode river_flow = RiverFlowMode::Flow;

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
/// Waterfalls and river mouths (FLUESSE 2702–2704; P: docs/render/iso.md,
/// "Water"). Enhanced water folds them into the river surface instead of
/// drawing their sprites ("Rivers into the sea"); original water draws them.
inline constexpr std::array<std::uint32_t, 3> kRiverFallAnlagen = {2702, 2703, 2704};
/// Enhanced water: river depth (HEIGHT units) inside a waterfall or mouth
/// shape, and the surface slope given to waterfalls for their rapids
/// (iso::river_shade). Tuning values (not original).
inline constexpr float kFallDepth = 3.0F;
inline constexpr float kFallRapids = 12.0F;
/// Pixels (zoom group 0) over which river water takes on the sea's look
/// where they meet. Tuning value (not original).
inline constexpr float kRiverBlend = 16.0F;
/// Shallow sea pixels (under kSeamDepth HEIGHT units) in a gap of at most
/// kRiverSeam pixels (zoom group 0) between river water and sea water join
/// the river surface (no sand line between them). Tuning values (not
/// original).
inline constexpr float kRiverSeam = 4.0F;
inline constexpr float kSeamDepth = 1.0F;
/// Enhanced river water never ends short of the painted riverbed, i.e. of the
/// river sprites drawn with the original's depth test: the blurred alpha of
/// their closed mask (the sprites are a 50 % checkerboard) is 1 inside and 0.5
/// at the edge of the bed; the water depth floor is (alpha - kBedEdge) *
/// kBedDepth HEIGHT units. Tuning values (not original).
inline constexpr float kBedEdge = 0.5F;
inline constexpr float kBedDepth = 4.0F;
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
    /// Which frame this is; frames with the same identity share one entry in
    /// the frame table (and one atlas copy). Unknown: keyed by the image.
    UnitFrameId id;
};

/// The render form of a decoded unit frame of figure entry `figure`, with the
/// identity that lets identical units share a frame-table entry.
inline UnitFrame to_unit_frame(const assets::unit::DecodedFrame& f, std::uint16_t figure,
                               bool shadow) {
    std::uint32_t colour = UnitFrameId::kNoColour;
    if (f.colour && !shadow) {
        colour = std::uint32_t{(*f.colour)[0]} << 16U | std::uint32_t{(*f.colour)[1]} << 8U |
                 (*f.colour)[2];
    }
    return {f.frame, f.image, UnitFrameId{true, figure, f.index, colour}};
}

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
    ~Scene();
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
    /// The frames chunk draw lists name (gpu-chunks.md 4.1), interned by the
    /// threads that build lists; ids never change meaning within a scene.
    mutable FrameTable frame_table;
    Lift lift;
    /// Ships and soldiers (empty unless set by the caller).
    std::vector<UnitSprite> units;
    /// The river model and its shading (docs/render/rivers.md). Set before
    /// the first render or river_flow call; not to be changed while chunks
    /// are being built.
    river::RiverConfig river_cfg;

    /// The flow of island `index` (rivers.md 3.1: the raster, phase A, and
    /// the solve, phase B): null for an island without rivers, when
    /// river_cfg.enabled is false, or when the build failed. With `wait` the
    /// caller builds it if nobody has (blocking; never on the window thread).
    /// Without, a flow that is not ready yet is queued on the flow builders
    /// (low priority threads, started on the first request) and `pending` is
    /// set: draw the legacy look now and ask river_flow_done() later.
    struct RiverFlowResult {
        std::shared_ptr<const river::RiverFlow> flow;
        bool pending = false;
    };
    [[nodiscard]] RiverFlowResult river_flow(std::size_t index, bool wait) const;
    /// Queues every island with rivers on the flow builders (scene load).
    void prefetch_river_flow() const;
    /// Test hook (tests only): while paused the flow builders start no new
    /// build (queued islands stay pending), so the "flow not ready" path can be
    /// exercised deterministically.
    void pause_river_flow_builders(bool pause) const;
    /// Test hook (never set in production): called on the building thread
    /// before the flow of an island is built; throwing makes that build fail,
    /// which is the "failed" state (no flow, final, never pending).
    std::function<void(std::size_t)> flow_build_fault;
    /// Whether the river flow is on: river_cfg.enabled and a configuration that
    /// passes RiverConfig::validate (checked once, at the first call; a refused
    /// one is reported on stderr and leaves the rivers on the legacy look).
    [[nodiscard]] bool river_active() const;
    /// Whether island `index` has river pieces.
    [[nodiscard]] bool has_river(std::size_t index) const {
        return !objects_[index].rivers.empty();
    }
    /// The flow lattice (rivers.md 3.3) for the pixels of `area` at `group`:
    /// the global lattice texels that cover `area` (texel pixels are multiples
    /// of the group's stride), baked from the ground probes at their pixels.
    /// `probe` is the ground probe of `probe_rect` (the chunk or region being
    /// built); texel pixels outside it (the apron: one stride past the right
    /// and bottom edge) get their own thin probes. Texels over islands without
    /// a flow stay invalid; with `wait` false an island whose flow is not
    /// ready adds that island to *waiting. An empty plane when no texel has a flow. A texel
    /// is a pure function of its pixel and the island's flow, so neighbouring
    /// chunks hold bit-identical texels where they overlap.
    [[nodiscard]] river::FlowPlane flow_plane(Rect area, std::size_t group,
                                              const sea::SceneProbe& probe, Rect probe_rect,
                                              bool wait, std::vector<std::uint32_t>* waiting) const;
    /// Whether island `index`'s flow is final (built, failed, or none): a
    /// chunk that waited for it can be rebuilt. Never blocks, queues nothing.
    [[nodiscard]] bool river_flow_done(std::size_t index) const;

    /// The zoom levels the scene is drawn at: the three built groups and the
    /// derived levels after them (default: the original three). Set before the
    /// scene is shared with other threads. Scene content (layouts, objects,
    /// units) exists for the built levels only; at a derived level `sea`, `bounds`
    /// and the sprite lists answer from the level above (reduced or empty).
    void set_levels(ZoomLevels levels);
    [[nodiscard]] const ZoomLevels& levels() const { return levels_; }

    /// The sea pattern of `group`, rendered on first use (thread-safe).
    const sea::SeaPattern* sea(std::size_t group) const;
    /// The sea pattern of `group` if it has been rendered already (never
    /// blocks).
    [[nodiscard]] const sea::SeaPattern* sea_if_ready(std::size_t group) const;
    /// Whether the sea pattern of `group` has been rendered (or turned out
    /// to be missing: sea_if_ready is then null); never blocks.
    [[nodiscard]] bool sea_built(std::size_t group) const;
    /// Whether island `index` has water objects below / above the waterline.
    [[nodiscard]] std::pair<bool, bool> has_water_objects(std::size_t index) const;
    /// The sprites of `layer` touching `rect` at time_ms, in draw order,
    /// anchors in scene coordinates.
    [[nodiscard]] std::vector<SpriteDraw> sprites_in(Rect rect, std::size_t group,
                                                     std::uint64_t time_ms, SpriteLayer layer,
                                                     WaterStyle style) const;
    /// Static sea data of `rect`.
    [[nodiscard]] sea::WaterField water_field(Rect rect, std::size_t group) const;
    /// Static sea data of `rect` from its ground probe at `group`; enhanced
    /// river water includes the shapes of waterfalls and mouths.
    [[nodiscard]] sea::WaterField water_field_from(Rect rect, const sea::SceneProbe& probe,
                                                   std::size_t group) const;
    /// QA/debug (tools/river_qa): the river, waterfall and river-mouth sprites
    /// that original water draws in `rect` (depth-tested against `probe`, the
    /// ground probe of `rect`, as render_with_terrain does), alpha > 0 where a
    /// sprite covers the pixel. Enhanced water must carry water at all of them.
    [[nodiscard]] Canvas river_sprites(Rect rect, const sea::SceneProbe& probe,
                                       std::size_t group) const;
    /// Chunk::terrain for the pixels of `probe`: terrain_at at each pixel in
    /// 1/kTerrainScale HEIGHT units, kNoTerrain where no ground was drawn.
    [[nodiscard]] std::vector<std::uint16_t> terrain_plane(const sea::SceneProbe& probe) const;
    /// QA/debug: the per-island water inputs (river level, smooth terrain).
    [[nodiscard]] const sea::IslandWater* island_water_info(std::size_t index) const {
        return island_water(index);
    }
    /// QA/debug: the river pieces (kRiverAnlagen) of island `index`.
    [[nodiscard]] std::span<const sea::RiverPiece> river_pieces(std::size_t index) const {
        return objects_[index].rivers;
    }
    /// The tile-space river raster of island `index` (rivers.md R1-R4): the
    /// shipped enhanced-water criteria (river level over the Catmull-Rom bed,
    /// the painted bed, waterfall and mouth shapes) sampled per cell. Not
    /// cached; builds the sprite covers at zoom group 0 on every call
    /// (30 ms p50, 84 ms p95, 96 ms max over the corpus). A pure function of
    /// the scene: safe from any thread (SpriteCache is mutex guarded), the
    /// same result for every caller. Call it once per island on a worker
    /// (std::call_once, rivers.md 3.1) and share the result; never from the
    /// window thread and never per chunk.
    [[nodiscard]] river::RiverMask river_mask(std::size_t index,
                                              const river::RiverConfig& cfg) const;
    /// QA/debug: river, waterfall and mouth pieces of island `index`.
    [[nodiscard]] std::size_t river_piece_count(std::size_t index) const;
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
    /// The draw list of the objects layer of `rect` at `group`: what render()
    /// with Layers::objects (and `ships`, `bases`) composites there, as records
    /// in blit order that replay() draws with the same result, byte for byte
    /// (not water objects: they are drawn per frame; time 0). Ships follow the
    /// islands as unclipped records. Decodes the frames it names on the calling
    /// thread (frame_table).
    [[nodiscard]] ChunkDraws objects_draws(Rect rect, std::size_t group, bool ships, bool bases,
                                           std::uint32_t bin_px = kDefaultBinPx) const;
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
        /// Waterfalls and mouths (kRiverFallAnlagen) under / at or above the
        /// sea surface: their sprites in original water, river-water shapes
        /// in enhanced water.
        std::vector<Prepared> falls_below, falls_above;
        /// The river pieces' sprites (kRiverAnlagen, at or above the sea
        /// surface): the extent of the painted riverbed (Scene::water_field_from).
        std::vector<Prepared> river_sprites;
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
    /// Enhanced river water in the shapes of the waterfall and mouth
    /// sprites (alpha masks of `falls` and `mouths`, rect-sized).
    void apply_fall_masks(sea::WaterField& f, const Canvas& falls, const Canvas& mouths,
                          const Canvas& beds, std::size_t group) const;
    /// River pixels near the sea: distance (shore byte) and cover blend.
    void blend_rivers_into_sea(sea::WaterField& f, std::size_t group) const;
    std::pair<DrawStats, std::size_t> draw_island_objects(Canvas& local, const Placement& p,
                                                          std::span<const Prepared> list,
                                                          std::size_t group) const;
    [[nodiscard]] std::optional<Rect> unit_rect(const UnitSprite& u, std::size_t group) const;
    std::size_t draw_units(Canvas& canvas, std::size_t group) const;

    struct FlowSlot;
    struct FlowBuild;
    void build_flow(std::size_t index) const;
    void queue_flow(std::size_t index) const;
    void start_flow_builders() const;

    mutable std::vector<IslandData> objects_;
    mutable std::array<SeaSlot, kGroups> seas_;
    ZoomLevels levels_ = ZoomLevels::original();
    /// The sea patterns of the derived levels (index level - built_count()).
    mutable std::deque<SeaSlot> derived_seas_;
    mutable std::vector<std::unique_ptr<FlowSlot>> flow_slots_;
    mutable std::unique_ptr<FlowBuild> flow_build_;
    mutable std::once_flag flow_build_once_;
    mutable std::once_flag river_cfg_once_;
    mutable bool river_cfg_ok_ = true;
};

} // namespace anno::render
