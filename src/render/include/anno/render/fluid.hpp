// Coastal wave simulation of the enhanced water (not original, opt-in).
// C++20 port of crates/render/src/fluid.rs (CPU part; the GPU host and
// fluid.wgsl follow), with the same float operations in the same order.
//
// **Render-only.** It reads the terrain, never game state, and nothing reads
// it back: it is outside the deterministic game simulation, so it cannot
// affect lockstep multiplayer. The GPU runs it (fluid.wgsl); this module
// holds the parameters (FluidConfig), the incoming swell (swell_components,
// forcing), the camera-local window (FluidWindow) and a CPU reference solver
// (FluidGrid) with the same arithmetic, which tests compare with the GPU.
//
// **Scales with the map:** the grid covers a fixed number of cells around
// the view (FluidConfig::size) and scrolls with the camera as a ring buffer
// (storage index = world cell index mod size), so only newly visible strips
// are initialised. Cost is independent of map size and island count.
//
// **Model** (spec: docs/render/iso.md, "Wave simulation"): linear
// shallow-water equations on a staggered grid, units HEIGHT units for
// surface elevation η and depth h, tiles for distance, seconds for time:
//
//     ∂u/∂t = −g ∂η/∂x − damping(h) u        (u on x faces, v on y faces)
//     ∂η/∂t = −∂(H u)/∂x − ∂(H v)/∂y          H = clamp(face depth, min, cap)
//
// with wave speed √(g H) (deep `deep_speed` at `depth_cap`), so waves slow
// down, steepen and turn towards the shore over shallows (shoaling,
// refraction), bend around islands (diffraction) and reflect. Where the
// elevation exceeds `break_ratio · h` the wave breaks: the excess is
// dissipated and becomes foam, which decays. Deep water and a sponge band at
// the window border are relaxed towards an incoming swell that is a sum of
// kSwellComponents plane waves with seeded random directions, periods and
// phases; their interference makes wave groups and an irregular sea. Wetting
// and drying: the flux uses the upwind total depth η + h (≥ 0), a face
// carries flow only where the higher surface lies above the higher bed, and
// the surface never drops below the bed — so bores run up the beach, slow
// down by friction and drain back (the run-up is simulated, not added).
// Swash flow churns foam. Integration is symplectic Euler (velocities, then
// elevation), explicit, stable for dt < dx / (√2 · deep_speed).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace anno::render::fluid {

/// Number of plane waves in the incoming swell (also the shader's array
/// size). Fixed because the shader's uniform array has this size; changing
/// it means changing fluid.wgsl too.
inline constexpr std::size_t kSwellComponents = 8;

/// Parameters of the simulation. Tuning values, not from the original; kept
/// together so they can become configuration (PLAN §0.1, §0.2). A
/// value-initialised FluidConfig holds the defaults (Rust FluidConfig::DEFAULT).
struct FluidConfig {
    /// Largest number of cells per side of the camera-local grid; the window
    /// covers the view (window_size).
    std::uint32_t size = 2048;
    /// Cells per tile per zoom group (finer when zoomed in).
    std::vector<std::uint32_t> cells_per_tile = {16, 8, 4};
    /// Depth (HEIGHT units) above which water counts as deep: the flux depth
    /// is capped here, so deep waves travel at deep_speed.
    float depth_cap = 16.0F;
    /// Speed of deep-water waves (tiles per second).
    float deep_speed = 3.0F;
    /// Smallest flux depth (stability over very shallow water).
    float min_depth = 0.5F;
    /// Time step (seconds) and most steps per frame (the simulation skips
    /// ahead when a frame takes longer).
    float dt = 1.0F / 120.0F;
    std::uint32_t max_steps = 12;
    /// Velocity damping per second, plus bottom friction scaled by 1 / H.
    float damping = 0.05F;
    float friction = 0.04F;
    /// Waves break where η exceeds break_ratio · h; the excess decays at
    /// break_rate per second and turns into foam (foam_gain per unit), which
    /// decays with time constant foam_decay seconds.
    float break_ratio = 0.8F;
    float break_rate = 8.0F;
    /// Breaking applies where the still depth exceeds this; shallower, in the
    /// swash zone, the broken wave runs on as a bore and only friction slows
    /// it (clipping there would stop all run-up).
    float break_min_depth = 3.0F;
    float foam_gain = 1.5F;
    float foam_decay = 1.6F;
    /// Deep water (depth force_depth.first → .second) relaxes towards the
    /// incoming swell at force_rate per second; within sponge_cells of the
    /// window border fully.
    std::pair<float, float> force_depth = {10.0F, 15.0F};
    float force_rate = 1.5F;
    std::uint32_t sponge_cells = 32;
    /// Largest flow speed (tiles per second; keeps thin swash stable).
    float max_speed = 2.5F;
    /// Foam churned by swash flow per tile of flow.
    float swash_foam = 0.6F;
    /// Swash water seeping into the beach above the waterline (HEIGHT units
    /// per second), so thin films fall dry instead of draining only
    /// asymptotically.
    float infiltration = 0.4F;
    /// Incoming swell: main direction (radians in tile axes), largest
    /// component amplitude (HEIGHT units), periods (seconds), seed.
    float swell_dir = 0.28F;
    float swell_amplitude = 1.2F;
    std::pair<float, float> swell_periods = {2.6F, 4.6F};
    std::uint32_t seed = 1503;
    /// The window recentres when the view centre moves this many cells.
    std::uint32_t recentre_cells = 64;

    /// Cells per side for a view spanning span_tiles tiles (screen width /
    /// tile width + screen height / tile height) at zoom `group` (<
    /// cells_per_tile.size()): the view plus room to recentre and the sponge
    /// on each side, rounded up to 64, at most `size` (`size` itself if it is
    /// below 64, where Rust's clamp panics).
    [[nodiscard]] std::uint32_t window_size(std::size_t group, float span_tiles) const;
    /// window_size for a window of `cells_per_tile` (> 0) cells per tile instead of
    /// the group's (bounded_cells_per_tile).
    [[nodiscard]] std::uint32_t window_size_for(std::uint32_t cells_per_tile,
                                                float span_tiles) const;

    /// Gravity in tiles² / (s² · HEIGHT unit), from deep_speed at depth_cap.
    [[nodiscard]] float gravity() const { return deep_speed * deep_speed / depth_cap; }

    bool operator==(const FluidConfig&) const = default;
};

/// One plane wave of the incoming swell: wave vector k (radians per tile),
/// angular frequency omega (radians per second), phase, amplitude.
struct SwellComponent {
    std::array<float, 2> k{};
    float omega = 0.0F;
    float phase = 0.0F;
    float amplitude = 0.0F;

    bool operator==(const SwellComponent&) const = default;
};

using Swell = std::array<SwellComponent, kSwellComponents>;

/// The incoming swell of `cfg`: directions spread around the full circle
/// with the main direction strongest (so every coast gets waves, the
/// windward side most), seeded random periods and phases.
Swell swell_components(const FluidConfig& cfg);

/// Elevation and depth-averaged velocity of the incoming swell.
struct Forcing {
    float eta = 0.0F, u = 0.0F, v = 0.0F;
};

/// Incoming swell at tile position (x, y) and time t (s): elevation and the
/// depth-averaged velocity of a progressive wave in deep water
/// (η · deep_speed / depth_cap along its direction).
Forcing forcing(const FluidConfig& cfg, const Swell& comps, std::pair<float, float> pos, float t);

/// A rectangle of world cells [x0, x1) × [y0, y1).
struct CellRect {
    std::int64_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;

    [[nodiscard]] bool is_empty() const { return x1 <= x0 || y1 <= y0; }
    [[nodiscard]] bool contains(std::int64_t x, std::int64_t y) const {
        return x >= x0 && x < x1 && y >= y0 && y < y1;
    }
    bool operator==(const CellRect&) const = default;
};

/// The camera-local window: size × size world cells starting at origin,
/// cells_per_tile cells per tile.
struct FluidWindow {
    std::int64_t size = 0;
    std::uint32_t cells_per_tile = 0;
    std::pair<std::int64_t, std::int64_t> origin{0, 0};

    /// A window of `cfg` at zoom `group` centred on tile position `centre`
    /// (Rust FluidWindow::new).
    static FluidWindow centred(const FluidConfig& cfg, std::size_t group,
                               std::pair<float, float> centre);
    /// As centred with `size` (> 0) cells per side.
    static FluidWindow with_size(const FluidConfig& cfg, std::size_t group,
                                 std::pair<float, float> centre, std::uint32_t size);
    /// As with_size with `cells_per_tile` (> 0) cells per tile instead of the group's.
    static FluidWindow with_cells(std::uint32_t cells_per_tile, std::pair<float, float> centre,
                                  std::uint32_t size);

    /// The whole window as a cell rectangle.
    [[nodiscard]] CellRect rect() const {
        return {origin.first, origin.second, origin.first + size, origin.second + size};
    }

    /// Recentres on `centre` if it moved at least `threshold` cells from the
    /// window centre; returns the cell rectangles that are new (at most two;
    /// the whole window after a jump).
    std::vector<CellRect> recentre(std::pair<float, float> centre, std::uint32_t threshold);

    /// Storage index (column, row) of world cell (x, y) (ring buffer).
    [[nodiscard]] std::pair<std::size_t, std::size_t> storage(std::int64_t x, std::int64_t y) const;

    /// Tile position of the centre of world cell (x, y).
    [[nodiscard]] std::pair<float, float> cell_centre(std::int64_t x, std::int64_t y) const;

    bool operator==(const FluidWindow&) const = default;

  private:
    [[nodiscard]] std::pair<std::int64_t, std::int64_t>
    origin_for(std::pair<float, float> centre) const;
};

/// The bound on the wave window's cells at far zoom (T14; docs/render/iso.md "Wave
/// simulation", "Bounded at far zoom"). The window covers the view's tiles at
/// cells_per_tile[group] cells per tile, so its cells grow with the world area shown: at
/// the continuous zoom's smallest scale on a level (0.45) up to 1 / 0.45² = 4.9 times its
/// cells at scale 1 for the same window inside its fixed margins (3.7 times with them
/// for a 1280 x 800 window), and the cost of a step with them (one compute pass over
/// every cell, docs/perf/README.md "T14"). The cells per tile for a view whose tiles are
/// `tile_px` screen pixels wide (the level's tile width times the resample scale) are
/// cells_per_tile[group] halved until at most `max_cells_per_px` cells fall on a screen
/// pixel along x (cells_per_tile / tile_px <= max_cells_per_px), so a cell is never
/// smaller on screen than 1 / max_cells_per_px pixels and the window's cells per screen
/// pixel stay bounded at every zoom; 0 when even one cell per tile is denser (the
/// simulation is off there: the shader's analytic waves). `max_cells_per_px` <= 0 = no
/// bound (the group's cells). With `current`, the cells per tile of the window in use (0 =
/// none), a finer window replaces a coarser one only when it fits with
/// kCellsHysteresis of the bound to spare, so a zoom resting near a threshold does not
/// switch windows back and forth.
std::uint32_t bounded_cells_per_tile(const FluidConfig& cfg, std::size_t group, double tile_px,
                                     double max_cells_per_px, std::uint32_t current = 0);
/// The share of the bound a finer window must fit in to replace a coarser one (5 %
/// room, as the continuous zoom's level choice; iso.md "Choosing the level").
inline constexpr double kCellsHysteresis = 0.95;
/// The default of the bound (GpuLimits::fluid_max_cells_per_px): 0.25 cells per screen
/// pixel. The built groups have 16 / 84, 8 / 42 and 4 / 20 = 0.19, 0.19 and 0.20 cells per
/// screen pixel at scale 1 (cells_per_tile over the tile width), so a quarter above the
/// densest keeps every group's own cells from scale 1 down to 0.8 (0.76 for groups 0 and
/// 1) and halves them once below that, to the far end of every level (0.45): the window's
/// cells, and the simulation's GPU time, stay at about their scale-1 figure there instead
/// of growing 3-4 times (measured on the Radeon iGPU, docs/perf/README.md "T14").
inline constexpr double kDefaultMaxCellsPerPx = 0.25;

/// Water deeper than this (HEIGHT units) counts as wet.
inline constexpr float kWet = 0.02F;

/// (η, u, v, foam) of one storage cell.
using Cell = std::array<float, 4>;

/// Elevation, velocity and foam of a freshly initialised cell. `h` is the
/// cell's still-water depth (negative on land): dry land gets the surface at
/// its bed (η = −h) and no flow.
Cell reset_cell(const FluidConfig& cfg, const Swell& comps, const FluidWindow& window,
                std::pair<std::int64_t, std::int64_t> cell, float t, float h);

/// Depth below the waterline (HEIGHT units, ≤ 0 on land) at a tile position.
using DepthFn = std::function<float(float, float)>;

/// CPU reference solver: the same arithmetic as fluid.wgsl, storage in
/// ring-buffer order. Used by tests (and small enough grids only).
class FluidGrid {
  public:
    FluidConfig cfg;
    Swell comps;
    FluidWindow window;
    std::vector<float> depth;
    /// (η, u, v, foam) per storage cell.
    std::vector<Cell> state;

    /// A grid over `window` with depth `depth(x, y)` (tile position → depth
    /// below the waterline in HEIGHT units, ≤ 0 on land), initialised to the
    /// incoming swell at time t.
    FluidGrid(FluidConfig cfg, FluidWindow window, const DepthFn& depth, float t);

    /// Sets depth and state of the cells of r (new cells).
    void fill(CellRect r, const DepthFn& depth, float t);

    /// One time step at simulation time t (s, after the step).
    void step(float t);

  private:
    /// Relaxation factor towards the swell for world cell (x, y) of depth h
    /// (sponge at the window border, deep water).
    [[nodiscard]] float force_factor(std::int64_t x, std::int64_t y, float h, float dt) const;
};

/// 0 below a, 1 above b, smooth in between (also for a > b); the shader's
/// sstep.
float smoothstep(float a, float b, float x);

} // namespace anno::render::fluid
