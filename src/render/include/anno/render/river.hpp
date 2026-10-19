// Enhanced rivers: the tile-space river raster and the steady flow solved on
// it (spec: docs/render/rivers.md, sections 2-3; stage S4, CPU only).
//
// RiverMask is the island raster of the enhanced river water: the SHIPPED
// gap-fix criteria (docs/render/iso.md, "River gaps") sampled on a grid of
// cells_per_tile x cells_per_tile cells per tile instead of on screen pixels:
// the physical river test of sea::water_field (sea::river_point: river level
// over the Catmull-Rom bed), the painted bed (the river sprites' own alpha,
// closed over the checkerboard) and the shapes of the waterfall and mouth
// sprites. RiverFlow is the depth-averaged flow on that raster: a steady
// diffusive wave with Manning conveyance, one SPD solve per connected piece of
// river (rivers.md 2.3), rescaled by a discharge model (2.4). Nothing here
// touches simulation state, draws, or runs on the window thread; both are
// immutable results of pure functions of their inputs and deterministic (no
// threads inside, fixed summation order).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <anno/render/iso.hpp>
#include <anno/render/sea.hpp>

namespace anno::render::river {

/// One HEIGHT unit in tiles (docs/render/iso.md, "Elevation": 0.04 world units).
inline constexpr float kUnitTiles = kHeightUnitTiles;

/// The largest lattice stride (pixels per texel) the GPU compositor can carry: its
/// chunk flag word keeps the stride in bits 8-15 (gpu.hpp kChunkStrideShift), so 8
/// bits. RiverConfig::validate refuses larger strides, so CPU and GPU never differ
/// in whether a lattice is drawn. To lift it, widen the stride field (the flag word
/// has bits 16-31 free) and raise this constant with the shader's decoding.
inline constexpr std::uint32_t kMaxLatticeStride = 255;

/// One layer of the river flow shading (rivers.md 2.8), see iso::RiverLayer.
struct LayerConfig {
    float scale = 1.0F, period_ms = 1000.0F, amplitude = 0.0F;
    /// The amplitude follows the zoom group's wave detail (0 at group 2).
    bool detail_fades = false;
    /// A rapids layer: lit by the surface slope, not an emboss ripple.
    bool rapids = false;
};

/// The default layer table (rivers.md 2.8; tuning values, none is evidence):
/// slow undulation, ripples, glints, and the whitewater streaks of falls.
/// The amplitudes are calibrated so that the light variance of the flow
/// shading equals the legacy shimmer's (+-20 %; test "the flow shading keeps
/// the legacy shimmer's light variance").
std::vector<LayerConfig> default_layers();

/// The Anlagen ids that are flow obstacles by default (RiverConfig::
/// obstacle_ids): rocks and cliffs that stand in rivers, reviewed on contact
/// sheets (rivers.md 17.2, OQ-091). None is evidence of a category: the data
/// has no object categories.
std::vector<std::uint32_t> default_obstacle_ids();

/// The tunables of the river model (PLAN 0.1: data, not constants). Every
/// value below is a tuning value or derived (rivers.md 2); none is evidence
/// from the original game, the data carries no discharge.
struct RiverConfig {
    /// Raster resolution (cells per tile and axis) of mask and flow.
    std::uint32_t cells_per_tile = 8;
    /// Margin (tiles) around the river pieces that is classified, so sea
    /// next to a river mouth is seen by the outlet rule.
    float mask_margin_tiles = 1.5F;

    /// Water depth bounds (HEIGHT units) of the hydraulic depth and the
    /// width (tiles) of the Gaussian that smooths the quantised painted bed
    /// (4-unit steps, 3 x 3 blocks) before it enters the conveyance.
    float h_min = 1.5F, h_max = 12.0F, bed_sigma_tiles = 0.75F;

    /// Manning roughness (s / m^(1/3); Chow 1959: 0.030-0.050), the metre
    /// length of a tile (rivers.md 2.1, derived from the sea simulation's
    /// gravity) and the depth exponent of the conveyance (5/3 Manning, 3/2
    /// Chezy).
    float manning_n = 0.035F, metres_per_tile = 0.698F, conveyance_exp = 5.0F / 3.0F;
    /// Bank roughness multiplier nu(d) = 1 + bank_roughness * exp(-d / bank_width)
    /// on the roughness at distance d (tiles) from a bank (rivers.md E6).
    float bank_roughness = 4.0F, bank_width_tiles = 0.30F;

    /// Speeds (tiles / s): the mean cross-section speed at the outlets of a
    /// river (Normalised discharge), the soft floor under every speed, the
    /// cap (the deep-water wave speed of the sea simulation) and the speed
    /// over waterfalls and rapids.
    float out_speed = 0.9F, speed_floor = 0.12F, speed_cap = 3.0F, fall_speed = 2.5F;
    /// Normalised: where the outlet speed is measured: the fastest cross-section
    /// (at least out_min_cells cells wide) that still passes out_flux_share of
    /// the river's discharge, so that the broad river-mouth sprites do not set
    /// the scale (rivers.md E7 measures at the sea faces).
    float out_flux_share = 0.5F;
    std::uint32_t out_min_cells = 4;
    /// Normalised: what out_speed is a speed of (decision OQ-065). Both keep
    /// the mean-speed law of a straight channel.
    enum class Reference {
        /// The fastest cross-section that still passes out_flux_share of the
        /// discharge gets out_speed (default): the lower course of a river
        /// runs at out_speed, tributaries scale down with the area they drain
        /// (most of a network sits at or near the soft floor).
        FastestSection,
        /// The median wet cell of a river gets median_speed (out_speed is
        /// ignored): a livelier, flatter field, the narrows reach the cap.
        MedianCell,
    } reference = Reference::FastestSection;
    /// MedianCell: speed (tiles / s) of the median cell of every river.
    float median_speed = 0.4F;

    /// How the discharge of a river is set (rivers.md 2.4, decision D2).
    enum class Discharge {
        /// Every river reaches out_speed at its outlets (default).
        Normalised,
        /// A fixed inflow per wet tile (inflow_rate): long rivers carry more.
        Absolute,
        /// Inflow only at detected heads (dead ends): source_speed times the
        /// cross-section there.
        Sourced,
    } discharge = Discharge::Normalised;
    /// Absolute: lateral inflow in tiles / s (volume per tile^2 and second).
    /// Tuned so that D_256x256_0045_5B reaches about out_speed at its outlets.
    float inflow_rate = 0.0030F;
    /// Sourced: speed (tiles / s) of the inflow at a head, and the least
    /// distance (tiles) of a head from the outlet.
    float source_speed = 0.25F, source_min_tiles = 2.0F;
    /// Sourced: how far (tiles) from the dead end the middle of the channel is
    /// looked for, to measure the width of the inflow.
    float source_reach_tiles = 2.0F;

    /// Rivers (4-connected pieces of water) smaller than this many cells are
    /// not solved: ponds with a static eddy of speed pond_swirl (tiles / s).
    std::uint32_t pond_cells = 12;
    float pond_swirl = 0.03F;

    /// Linear solver: PCG with an aggregation multigrid V-cycle
    /// preconditioner; the relative residual it must reach, the iteration
    /// cap, and the residual above which the Eikonal fallback replaces the
    /// solution of a river (rivers.md 2.3). The coarsest level is solved
    /// directly below coarsest_cells cells; max_levels bounds the hierarchy.
    double cg_tolerance = 1e-5;
    std::uint32_t cg_max_iterations = 400;
    double residual_limit = 1e-3;
    std::uint32_t coarsest_cells = 96, max_levels = 24;
    /// The coarsest level is factored densely (Cholesky, n^3 / 3 flops and
    /// 8 n^2 bytes: 9e9 and 72 MB at 3000 cells) when it has at most
    /// dense_limit cells; a larger one (stagnating coarsening, never seen:
    /// the last level of the corpus has at most 96 cells) is smoothed with
    /// coarse_sweeps damped Jacobi sweeps instead.
    std::uint32_t dense_limit = 3000, coarse_sweeps = 60;
    /// Bed smoothing is a diffusion of ceil(2 sigma^2) passes (cells); a
    /// sigma that would need more passes is clamped to this many (a
    /// narrower Gaussian than asked; 2000 passes = sigma 31 cells, 4 tiles at
    /// the default 8 cells per tile).
    std::uint32_t bed_smoothing_max_passes = 2000;

    /// Wetness-weighted 3 x 3 smoothing passes over the velocity field.
    std::uint32_t smoothing_passes = 1;
    /// Extra passes of speed-weighted smoothing over the cells within
    /// transition_reach cells (4-neighbour steps) of a fall, mouth or bridge
    /// cell: the speed changes from the river's to the fall's within less
    /// than a tile there (rivers.md 14.6, review S4).
    std::uint32_t transition_passes = 4, transition_reach = 8;

    /// Longest channel (tiles) carved to link the rivers that meet at a
    /// waterfall or river mouth (0 = none); see RiverMask::from_island.
    float bridge_tiles = 2.0F;
    /// The carved channel has the cells within this many steps (|dx| + |dy|)
    /// of its centreline (1 = a plus-shaped brush, three cells wide), and is
    /// at least bridge_min_depth HEIGHT units deep (the fall sprites cover too
    /// little to measure the depth there: a shallow one would carry no flow).
    std::uint32_t bridge_half_width = 1;
    float bridge_min_depth = 3.0F;

    /// Rivers without sea contact drain to a pseudo outlet, a guess (OQ-067).
    /// Their solved velocities are multiplied by this: 1 = flow as solved
    /// (default), 0 = still water (like a pond, without the swirl). The cells
    /// carry RiverFlow::kSeaLess either way, so a renderer can fade them.
    float sea_less_speed_scale = 1.0F;

    /// Print one line to stderr for every river that fell back.
    bool log_fallback = true;

    // --- Stage S5: lattice bake and flow shading (rivers.md 3, 8) ---

    /// Bake the flow lattice into chunks and let the enhanced path shade with
    /// it. false: chunks carry none and enhanced rivers keep the legacy
    /// shimmer (the viewer's --river-flow legacy / key R choose the look at
    /// run time; this is the master switch of the work).
    bool enabled = true;
    /// Reach (tiles) of the extension beyond the wet cells (rivers.md R9): the
    /// flow of the nearest wet cell with a weight fading from 1 to 0, so that
    /// the soft edge, sprite-only pixels and the falls and mouth masks, which
    /// reach a little past the raster, always find a flow. The extension grid
    /// has (nx + 2 reach) x (ny + 2 reach) cells of 4 bytes per island, so
    /// validate() caps the reach at kMaxExtendTiles (a 16-tile reach on the
    /// largest raster is 17 MB); raise the constant to lift the cap.
    float extend_tiles = 2.0F;
    /// The velocity gradient (shear, 1/s) of a lattice texel is the forward
    /// difference of the island's flow over this many tiles along each tile
    /// axis (about two raster cells: smooth enough not to see the cell grid,
    /// short enough to see a one-tile river's banks).
    float shear_probe_tiles = 0.25F;
    /// Threads of the flow builders (0 = automatic: a quarter of the hardware
    /// threads, at least 1, at most 4: the solves are short and the window
    /// thread and the chunk builders come first).
    std::uint32_t flow_threads = 0;
    /// Scene pixels per lattice texel at each zoom group; every stride must
    /// divide the chunk size (render::kChunk). 8 / 4 / 4 px are 0.10 / 0.10 /
    /// 0.20 tile at the original's tile widths. A thinner river would take 2.
    std::vector<std::uint32_t> lattice_stride = {8, 4, 4};
    /// Velocity range of the int16 lattice (tiles / s); 1/4096 resolution.
    float velocity_max = 8.0F;
    /// Below this weight of valid texels a pixel falls back to the legacy shimmer.
    float min_valid = 0.02F;
    /// Constant brightening of a full rapids layer and the shear fade of the
    /// layers (T |grad u| from shear_from to shear_to), see RiverParams.
    float rapids_base = 0.05F, shear_from = 0.5F, shear_to = 1.5F;
    /// The layer table, at most kMaxRiverLayers.
    std::vector<LayerConfig> layers = default_layers();
    /// Multiplies the amplitude of the ripple layers (not the rapids): 1 keeps
    /// the light variance of the legacy shimmer (the calibration, a design
    /// rule); larger values make the flow easier to see (decision for the
    /// owner, docs/open-questions.md).
    float light_gain = 1.0F;

    // --- Stage S7: hydraulic extras (rivers.md 2.5-2.7, 17) ---

    /// Gravity in tiles / s^2: the wave simulation's deep_speed^2 / (depth_cap *
    /// kUnitTiles) = 3^2 / (16 * 0.04) (rivers.md 2.1, FluidConfig::gravity()
    /// per HEIGHT unit over kUnitTiles; a test compares them). Froude number
    /// Fr = |u| / sqrt(g h) (E12).
    float gravity = 14.0625F;

    /// Obstacles (rivers.md 2.6, D4): objects that stand in the river become
    /// holes of the hydraulic domain. An object qualifies when its Anlagen id
    /// is in obstacle_ids (the list reviewed on contact sheets, OQ-073), its
    /// footprint has at most obstacle_max_area tiles and at least
    /// obstacle_min_wet of the footprint's cells are wet. The hole is the wet
    /// cells of the footprint shrunk by obstacle_shrink about its centre. An
    /// object whose hole would cut its river in two (a dam), or leave a way
    /// narrower than obstacle_gap_cells cells (a slit makes a jet), is left out
    /// and counted (RiverMask::obstacles). false: no holes.
    bool obstacles = true;
    std::vector<std::uint32_t> obstacle_ids = default_obstacle_ids();
    std::uint32_t obstacle_max_area = 16, obstacle_gap_cells = 3;
    float obstacle_min_wet = 0.5F, obstacle_shrink = 0.15F;
    /// Wake (lee) channel (rivers.md 2.6): a cell whose upstream trace of
    /// wake_length * a_eff tiles (a_eff = radius of the disc with the
    /// footprint's area) meets a hole gets wake = exp(-t / (wake_length *
    /// a_eff)), t the distance to the hole; its speed is multiplied by
    /// 1 - wake_slow * wake. false: no wake (the holes still deflect the flow).
    bool wake = true;
    float wake_length = 8.0F, wake_slow = 0.6F;

    /// Turbulence (rivers.md 2.7, E16): the weights of the bank shear layer
    /// (reaching turb_bank_tiles from the bank), the change of direction over
    /// +-0.5 tile (per radian) and the rapids (Froude from turb_fr_from to
    /// turb_fr_to). Tuning values.
    float turb_bank = 0.35F, turb_bank_tiles = 0.4F, turb_bend = 0.5F, turb_rapids = 0.5F;
    float turb_fr_from = 0.45F, turb_fr_to = 0.95F;

    /// Picard refinement of the Manning conveyance (rivers.md 2.3, D1b): the
    /// true law K = k_M h^(5/3) / (nu sqrt(|grad phi|)) by up to
    /// picard_iterations further solves (0 = the linear conveyance, default).
    /// Guarded: the slope is floored at picard_floor of the river's mean slope
    /// (the data is flat in places), the update is damped by picard_relax, an
    /// iteration that does not converge within picard_tolerance (relative change
    /// of the potential) by the cap, or whose solve fails, leaves that river with
    /// its linear solution (FlowStats::picard_fallbacks).
    std::uint32_t picard_iterations = 0;
    float picard_floor = 0.05F, picard_relax = 0.7F, picard_tolerance = 1e-3F;

    /// Thalweg shift (rivers.md 2.5, E14, D12): in a bend the speed is moved
    /// from the inner to the outer bank by 1 + thalweg_beta * tanh(|kappa| W) *
    /// n_hat (kappa the curvature of the streamlines in 1/tile, W the width of
    /// the channel, n_hat in [-1, 1] across the channel, positive towards the
    /// outer bank), renormalised so that the mean over the cross-section
    /// keeps. Off: potential flow, whose maximum sits at the inner bank.
    bool thalweg_shift = false;
    float thalweg_beta = 0.35F;

    /// Shading of the extras (rivers.md 2.5, 2.6, 2.8), see RiverParams.
    /// Streaks and aeration follow max(slope rule, Froude rule): Froude from
    /// froude_from to froude_to; aeration is the strength of the tint toward
    /// the aeration colour at full rapids (never white); a cell in a wake is
    /// darkened by wake_dark and its streaks raised by wake_boost. Standing
    /// waves (off by default, D3): light bands of amplitude standing_amp where
    /// the Froude number reaches standing_from..standing_to, wavelength
    /// 2 pi u^2 / g clamped to standing_min..standing_max tiles (E13).
    float froude_from = 0.55F, froude_to = 0.95F;
    float aeration = 0.35F, wake_dark = 0.06F, wake_boost = 0.6F;
    bool standing_waves = false;
    float standing_amp = 0.05F, standing_from = 0.5F, standing_to = 1.0F;
    float standing_min = 0.45F, standing_max = 2.5F;

    /// Empty when the shading tables are consistent: every layer period
    /// divides 3,600,000 ms (the hourly time wrap), at most kMaxRiverLayers
    /// layers, one positive lattice stride per zoom group dividing the chunk
    /// size `chunk_px` and are at most kMaxLatticeStride, finite positive scales.
    /// Otherwise the first problem.
    [[nodiscard]] std::string validate(std::uint32_t chunk_px, std::size_t groups) const;
    /// The shading uniform of the table (layers beyond the maximum dropped).
    [[nodiscard]] RiverParams params() const;
};

/// The baked flow of a rectangle of scene pixels at one zoom group (rivers.md
/// 3.3, 8): a GLOBAL lattice, texel (i, j) at scene pixel (x0 + i * stride, y0
/// + j * stride) (the texel stands for the pixel with that corner; a pixel
/// samples at its centre, +0.5), so every chunk and every region reads the same
/// texels at the same pixels: the texel value is a pure function of the probe
/// pixel (the tile position the ground shows there) and the island's flow.
struct FlowPlane {
    std::int32_t x0 = 0, y0 = 0;
    std::uint32_t stride_px = 4, nx = 0, ny = 0;
    /// Tile size (pixels) of the zoom group the plane was baked at.
    std::int32_t tile_w = 84, tile_h = 42;
    /// 2 per texel: velocity along the tile axes, 1/4096 tiles / s.
    std::vector<std::int16_t> uv;
    /// 4 per texel: valid weight (0-255), turbulence, wake (0 until the
    /// hydraulic extras), river surface level in kLevelStep HEIGHT units.
    std::vector<std::uint8_t> aux;
    /// 1 per texel: the shear |grad u| in kShearStep 1/s units (baked from the
    /// island's flow, not from the neighbouring texels, so that it is smooth
    /// over texel borders and equal in neighbouring chunks).
    std::vector<std::uint8_t> shear;
    [[nodiscard]] bool empty() const { return nx == 0 || ny == 0; }
    [[nodiscard]] std::size_t texel(std::uint32_t i, std::uint32_t j) const {
        return std::size_t{j} * nx + i;
    }
};
/// Level resolution of FlowPlane::aux[3] (HEIGHT units per step), the
/// velocity resolution of FlowPlane::uv (steps per tile / s) and the shear
/// resolution of FlowPlane::shear (1/s per step).
/// The level byte holds 255 steps = 127.5 HEIGHT units; the original's
/// rivers top out at 121 (112 + the 9 of kRiverLevel), a mod with higher
/// water would clip: widen the byte to 16 bits together with the GPU texture
/// (S6) or raise the step. The shear byte holds up to 8 1/s; the shear fade
/// (shear_to / period) is complete long before.
inline constexpr float kLevelStep = 0.5F;
inline constexpr float kVelocityScale = 4096.0F;
inline constexpr float kShearStep = 1.0F / 32.0F;
/// Upper bound of RiverConfig::extend_tiles (see there).
inline constexpr float kMaxExtendTiles = 16.0F;

/// Writes one texel (quantising); `valid` 0-1, `level` HEIGHT units, `shear`
/// 1/s.
void set_texel(FlowPlane& plane, std::uint32_t i, std::uint32_t j, float ux, float uy, float valid,
               float turb, float wake, float level, float velocity_max, float shear = 0.0F);
/// The flow at scene pixel (sx, sy): valid-weighted bilinear interpolation of
/// the four texels around the pixel centre, (1-a)(1-b) weights times valid
/// normalised by their sum (a texel with valid = 0 does not drag the vector
/// or the shear to zero); valid = Sum of weight * valid (0 for pixels outside
/// the plane).
FlowSample flow_at(const FlowPlane& plane, int sx, int sy);

/// Cell flags of a RiverMask.
inline constexpr std::uint8_t kWet = 1;     ///< river water
inline constexpr std::uint8_t kSea = 2;     ///< sea (open water), not river
inline constexpr std::uint8_t kMouth = 4;   ///< inside a river-mouth sprite (2703, 2704)
inline constexpr std::uint8_t kFall = 8;    ///< inside a waterfall sprite (2702)
inline constexpr std::uint8_t kSprite = 16; ///< wet only through a sprite, not the terrain test
inline constexpr std::uint8_t kBridge = 32; ///< channel carved across a gap at a fall or mouth
inline constexpr std::uint8_t kHole =
    64; ///< wet cell under an obstacle (an object in the river): no flow

/// An object that may stand in the river as an obstacle (rivers.md 2.6): its
/// Anlagen id and its footprint in island tiles, [x, x + w) x [y, y + h).
struct Obstacle {
    std::uint32_t id = 0;
    int x = 0, y = 0, w = 1, h = 1;
};

/// What RiverMask::from_island made of an Obstacle.
struct ObstacleInfo {
    Obstacle object;
    /// Fraction of the footprint's cells that are wet.
    float wet_share = 0;
    /// Hole cells set (0 when the object was not used).
    std::size_t hole_cells = 0;
    /// Radius (tiles) of the disc with the footprint's area (a_eff).
    float radius = 0;
    enum class Result : std::uint8_t {
        Hole,    ///< used
        NotWet,  ///< less of the footprint is wet than obstacle_min_wet
        TooBig,  ///< footprint above obstacle_max_area
        Dam,     ///< the hole would cut the river in two
        NoCells, ///< the shrunk footprint holds no wet cell
    } result = Result::NotWet;
};

/// What a river sprite contributes to the raster (computed by Scene from the
/// sprite's alpha at zoom group 0; rivers.md R1).
struct SpriteCover {
    enum class Kind : std::uint8_t { Bed, Fall, Mouth } kind = Kind::Bed;
    std::uint8_t height = 0;
    /// Tile rectangle [tx0, tx1) x [ty0, ty1) covered by `cells`.
    int tx0 = 0, ty0 = 0, tx1 = 0, ty1 = 0;
    /// 1 where the sprite (closed over its checkerboard, depth-tested as the
    /// original) covers the cell; row-major over the rectangle's cells
    /// ((tx1 - tx0) * cells_per_tile per row).
    std::vector<std::uint8_t> cells;
};

/// The pure steps that turn a river sprite into SpriteCover cells (used by
/// Scene::river_mask, exposed so that they can be tested without the game
/// files; rivers.md 3.2, R1).
namespace detail {

/// Half extent (pixels) of the coverage window of a cell: the odd number of
/// pixels nearest to half of the cell's projection (tile_px / cells_per_tile
/// pixels): 2 for 84 px at 8 cells (a 5-pixel window), 1 for 42 px (3).
int half_window(float tile_px, std::uint32_t cells_per_tile);

/// The terrain point of a cell projected to island-local pixels at the
/// zoom group of the sprite (sx = (x - y) half_w, sy = (x + y) half_h -
/// lift_px terrain height), and whether the original's river depth test lets
/// the sprite pass there (terrain height <= pass_height; always when the
/// test is off).
struct CellPixel {
    float sx = 0, sy = 0;
    bool pass = true;
};

/// CellPixel of every cell of the tile rectangle [tx0, tx1) x [ty0, ty1),
/// row-major.
std::vector<CellPixel> project_cells(std::span<const std::uint8_t> heights, std::size_t w,
                                     std::size_t h, int tx0, int ty0, int tx1, int ty1,
                                     std::uint32_t cells_per_tile, float half_w, float half_h,
                                     float lift_px, bool depth_test, float pass_height);

/// A pixel rectangle and one alpha byte per pixel (row-major).
struct AlphaPlane {
    int x0 = 0, y0 = 0, w = 0, h = 0;
    std::vector<std::uint8_t> alpha;
};

/// The smallest pixel rectangle that holds every cell with its window (rx, ry),
/// the 3 x 3 closing and a pixel of rounding.
AlphaPlane plane_for(std::span<const CellPixel> cells, int rx, int ry);

/// 1 where the sprite covers a cell: the sprite's alpha (>= 128) closed over
/// 3 x 3 pixels (the sprites are a 50 % checkerboard) covers at least half
/// of the (2 rx + 1) x (2 ry + 1) window around the cell's pixel; 0 for cells
/// the depth test blocked.
std::vector<std::uint8_t> cover_from_alpha(std::span<const CellPixel> cells,
                                           const AlphaPlane& plane, int rx, int ry);

} // namespace detail

/// The river raster of one island (phase A).
class RiverMask {
  public:
    RiverMask() = default;
    /// An all-dry raster of nx x ny cells whose cell (0, 0) is island cell
    /// (x0, y0); cell (cx, cy) spans tiles [cx / c, (cx + 1) / c) per axis.
    RiverMask(std::uint32_t cells_per_tile, std::int32_t x0, std::int32_t y0, std::uint32_t nx,
              std::uint32_t ny);

    /// The raster of island `iw` from its river pieces and sprite covers; the
    /// objects of `obstacles` that qualify (RiverConfig::obstacles and its
    /// rules; the caller selects by id) become holes (kHole) at the end.
    static RiverMask from_island(const sea::IslandWater& iw,
                                 std::span<const sea::RiverPiece> pieces,
                                 std::span<const SpriteCover> covers, const RiverConfig& cfg,
                                 std::span<const Obstacle> obstacles = {});

    [[nodiscard]] std::uint32_t cells_per_tile() const { return c_; }
    [[nodiscard]] std::int32_t x0() const { return x0_; }
    [[nodiscard]] std::int32_t y0() const { return y0_; }
    [[nodiscard]] std::uint32_t nx() const { return nx_; }
    [[nodiscard]] std::uint32_t ny() const { return ny_; }
    /// Index of island cell (cx, cy); npos outside the raster.
    [[nodiscard]] std::size_t index(std::int32_t cx, std::int32_t cy) const;
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);
    [[nodiscard]] bool wet(std::int32_t cx, std::int32_t cy) const;
    [[nodiscard]] std::uint8_t flags_at(std::int32_t cx, std::int32_t cy) const;
    [[nodiscard]] std::size_t wet_count() const;

    /// Sets a cell: wet with bed and surface level (HEIGHT units), plus
    /// extra flags. Outside the raster: ignored.
    void set_wet(std::int32_t cx, std::int32_t cy, float bed, float level, std::uint8_t extra = 0);
    /// Marks a dry cell as sea (open water, an outlet for its wet neighbours).
    void set_sea(std::int32_t cx, std::int32_t cy);
    /// Surface level of a dry cell (the level exists a little beyond the
    /// water; used for the gradient at the edge). Outside: ignored.
    void set_level(std::int32_t cx, std::int32_t cy, float level);

    std::vector<std::uint8_t> flags;
    /// Bed height (HEIGHT units) of wet cells; 0 elsewhere.
    std::vector<float> bed;
    /// River surface level (HEIGHT units); NaN where there is none.
    std::vector<float> level;
    /// Channels carved at falls and mouths by from_island.
    std::size_t bridges = 0;
    /// What became of every object given to from_island.
    std::vector<ObstacleInfo> obstacles;

  private:
    std::uint32_t c_ = 8;
    std::int32_t x0_ = 0, y0_ = 0;
    std::uint32_t nx_ = 0, ny_ = 0;
};

/// Results of RiverFlow::build for logs, tests and QA (rivers.md 3.3).
struct FlowStats {
    std::size_t wet_cells = 0;
    /// Rivers (4-connected wet pieces) with at least pond_cells cells, and
    /// the smaller ponds.
    std::size_t components = 0, ponds = 0;
    /// Sea-facing cell faces (the outlets), and rivers without any: their
    /// outlet is a pseudo outlet at the lowest level (decision D14, OQ-067).
    std::size_t outlet_faces = 0, sea_less_components = 0;
    /// Rivers whose solution was replaced by the Eikonal field.
    std::size_t fallback_components = 0;
    bool fell_back = false;
    /// Most iterations any river needed, and the multigrid levels of the
    /// largest river.
    std::uint32_t iterations = 0, levels = 0;
    /// Worst relative residual |b - A phi| / |b| over all rivers (after the
    /// fallback decision: the fallback rivers are not counted, they have no
    /// solution).
    double residual = 0;
    /// Worst imbalance between the discharge entering the rivers and leaving
    /// them over the outlet faces, relative to the inflow.
    double flux_imbalance = 0;
    /// Speed percentiles over all wet cells (tiles / s), and the share of
    /// cells whose velocity points down the Eikonal distance to the outlets.
    float speed_mean = 0, speed_p50 = 0, speed_p95 = 0, speed_max = 0;
    /// Share of the cells of solved rivers whose velocity points down the
    /// Eikonal distance to the outlets, over all rivers and over the rivers
    /// with a sea face only. The distance is the field the solver itself uses
    /// as its fallback direction, and a sea-less river is measured against
    /// its own pseudo outlet: the figure shows that the solve points to the
    /// nearest outlet and never a wrong-way stretch (docs/render/rivers.md
    /// 14.3); braids and loops are covered by the unit tests.
    float downstream_fraction = 0, downstream_fraction_sea = 0;
    /// Share of the cells of solved rivers whose model speed lies below
    /// speed_floor (the floor sets their speed), and whose speed is set by
    /// speed_cap, before the smoothing passes.
    float floor_fraction = 0, cap_fraction = 0;
    /// Non-finite values found in the result (must stay 0).
    std::size_t non_finite = 0;
    /// Stage S7: hole cells (obstacles), cells with a wake, rivers solved with
    /// the Picard refinement and rivers that kept the linear solution because
    /// it did not converge or the solve failed, the largest number of Picard
    /// steps, cells moved by the thalweg shift.
    std::size_t hole_cells = 0, wake_cells = 0, picard_rivers = 0, picard_fallbacks = 0,
                thalweg_cells = 0;
    std::uint32_t picard_steps = 0;
    /// Wall time of build (diagnostic only; never part of the result).
    double build_ms = 0;
};

/// One river (4-connected piece of water with at least pond_cells cells) of
/// a flow field, for lists and reviews.
struct RiverInfo {
    std::size_t cells = 0;
    /// Bounding box in island cells, inclusive.
    std::int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    /// No sea face: the direction comes from a pseudo outlet (OQ-067).
    bool sea_less = false;
    /// The cell of the pseudo outlet (sea-less rivers).
    std::int32_t pseudo_cx = 0, pseudo_cy = 0;
};

/// The flow field of one island (phase B).
class RiverFlow {
  public:
    struct Cell {
        std::int32_t cx = 0, cy = 0;
        /// Depth-averaged velocity along the island's tile x and y axes (tiles / s).
        float ux = 0, uy = 0;
        /// Hydraulic depth (tiles), after clamping and smoothing.
        float depth = 0;
        /// River surface level (HEIGHT units) of the raster.
        float level = 0;
        /// Index of the river the cell belongs to; kPond for ponds.
        std::uint32_t river = 0;
        /// Surface slope |grad Z| (HEIGHT units per tile).
        float slope = 0;
        /// Stage S7: turbulence 0-1 (E16), wake 0-1 (2.6) and the Froude
        /// number |u| / sqrt(g h) (E12).
        float turb = 0, wake = 0, froude = 0;
        std::uint8_t flags = 0;
    };
    static constexpr std::uint32_t kPond = 0xFFFFFFFFU;
    /// Cell flags.
    static constexpr std::uint8_t kOutlet = 1;    ///< has a sea-facing face
    static constexpr std::uint8_t kFallback = 2;  ///< Eikonal field (solve failed)
    static constexpr std::uint8_t kPseudo = 4;    ///< pseudo outlet of a sea-less river
    static constexpr std::uint8_t kSeaLess = 8;   ///< a cell of a river without sea contact
    static constexpr std::uint8_t kHoleCell = 16; ///< under an obstacle: u = 0, no flow through it

    struct Sample {
        std::array<float, 2> u{0, 0};
        /// Weight of the wet cells in the bilinear footprint (0-1); u is
        /// normalised by it, so a pixel at the rim still gets a full vector.
        float valid = 0;
        /// River surface level (HEIGHT units), weighted the same way.
        float level = 0;
        /// Turbulence and wake (0-1), weighted the same way.
        float turb = 0, wake = 0;
    };

    /// The flow on `mask`. Pure and deterministic.
    static RiverFlow build(const RiverMask& mask, const RiverConfig& cfg);

    /// Velocity at tile position (tx, ty) (island tile coordinates): valid-
    /// weighted bilinear interpolation between the centres of the wet cells.
    [[nodiscard]] Sample sample(float tx, float ty) const;
    /// sample() extended past the wet cells (rivers.md R9): where the
    /// bilinear footprint holds fewer wet cells than a full one, the flow and
    /// level of the nearest wet cell within `extend_tiles` (the RiverConfig
    /// value the flow was built with) fill in, and `valid` rises to a weight
    /// that fades from 1 at the cell to 0 at the reach. Equal to sample()
    /// where that is already full. A pure function of the position.
    [[nodiscard]] Sample sample_extended(float tx, float ty) const;
    [[nodiscard]] const FlowStats& stats() const { return stats_; }
    /// Wet cells in raster order (row by row).
    [[nodiscard]] std::span<const Cell> cells() const { return cells_; }
    /// The rivers in the order of Cell::river.
    [[nodiscard]] std::span<const RiverInfo> rivers() const { return rivers_; }
    /// Index into cells() of island cell (cx, cy), or npos.
    [[nodiscard]] std::size_t cell_at(std::int32_t cx, std::int32_t cy) const;
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);
    [[nodiscard]] std::uint32_t cells_per_tile() const { return c_; }

  private:
    void build_extension(float extend_tiles);
    std::uint32_t c_ = 8;
    std::int32_t x0_ = 0, y0_ = 0;
    std::uint32_t nx_ = 0, ny_ = 0;
    std::vector<std::int32_t> grid_;
    /// The extension (R9): the nearest wet cell of every cell within
    /// extend_cells_ steps of the wet ones, over the bounding box grown by
    /// that margin; -1 elsewhere.
    std::int32_t ext_x0_ = 0, ext_y0_ = 0, extend_cells_ = 0;
    std::uint32_t ext_nx_ = 0, ext_ny_ = 0;
    float extend_tiles_ = 0;
    std::vector<std::int32_t> ext_;
    std::vector<Cell> cells_;
    std::vector<RiverInfo> rivers_;
    FlowStats stats_;
};

} // namespace anno::render::river
