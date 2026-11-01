// The ship route engine (task MAP-NAV-2, package C): a PURE, STATELESS, deterministic integer
// port of the original global route engine (AnnoWelt.dll world +0x6ab0, ctor 0x10012030,
// Run 0x10011500, Trace 0x10011640; docs/re/ship-navigation.md section 9,
// docs/sim/ship-navigation.md sections 5, 12.3, 12.6). It owns no simulation state, reads no clock,
// draws no random number and keeps no global: every call is a function of its arguments, so any
// number of threads may plan on the same immutable inputs.
//
// Two levels, as in the original:
//   * a coarse map of 8 x 8-tile cells (CoastIndex: a 12-bit edge-connectivity mask and a cost type
//     per cell, stored in 64 x 64-tile sectors, a null sector = open water 0xfff), searched by a
//     FIFO Dijkstra with label-at-discovery;
//   * a fine Dijkstra at one-tile resolution (8 neighbours NW,NE,N,SW,SE,S,W,E; 10/14; no corner
//     cutting) confined to the coarse path's blocks, with up to `retry_cap` learned coarse masks
//     when the fine level cannot get through.
// The goal is a Q24.8 point with an acceptance radius; the end tile is the reachable water tile of
// the goal disc with the best score 2304 - trunc(64 d^2), ties decided by ship-density stamps.
//
// Everything that the original leaves unbounded is bounded here by RouteLimits (work in node pops
// and accounted bytes); exceeding a limit returns BudgetExceeded and never a partial route.
#pragma once

#include <array>
#include <compare>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace anno::sim {

// ---------------------------------------------------------------------------------------------
// Limits and costs
// ---------------------------------------------------------------------------------------------

/// Per-query limits. Every field has the original's value as its default except the three work
/// limits, which the original does not have (it floods the window). Their defaults are at least 8x
/// the largest legal query measured on the real candidates and on the original-sized window of
/// 256 x 256 cells, and the coarse default exceeds the most pops the original-sized window can
/// ever cost (9 searches of 65,536 cells), so a query that the original completes on its window is
/// never refused by default (docs/sim/ship-route-limits.md). validate() is the only gate:
/// plan_route refuses invalid limits.
struct RouteLimits {
    /// Half-width of the start-centred window in tiles (ship +0x6e, original 0x400 = 1024).
    std::int32_t window_radius_tiles = 1024;
    /// The window is at most this many 64-tile blocks per axis, measured from the 8-aligned
    /// origin (SetWindow 0x1000edd0 rejects more than 32). A larger window is a limit failure.
    std::uint32_t max_window_blocks = 32;
    /// Goal acceptance radius in tiles (ctx radius 6.0 written by 0x10030b90).
    std::int32_t goal_radius_tiles = 6;
    /// Longest run of equal directions in the output (Trace maxRun, 10).
    std::uint32_t run_cap = 10;
    /// Learned coarse masks per query (0x10013fb0 cap, 8).
    std::uint32_t retry_cap = 8;
    /// Density stamps honoured per query (the registry query capacity, 0x40).
    std::uint32_t unit_cap = 64;
    /// Coarse node pops over the whole query, all retries included.
    std::uint64_t coarse_budget = 600000;
    /// Fine node pops over the whole query, all retries included.
    std::uint64_t fine_budget = 4000000;
    /// Accounted working-set bytes (logical sizes of the coarse and fine working sets, the learned
    /// list and the result runs; never container capacities, which differ between standard
    /// libraries).
    std::uint64_t memory_bytes = 64ull * 1024 * 1024;
};

/// Empty when the limits are valid, else a one-line diagnostic.
[[nodiscard]] std::string validate(const RouteLimits& limits);

/// Coarse costs of one navigation class: the cost of an orthogonal or a diagonal step into a coarse
/// cell of type 0 (open) and of type 3 (shore band). A straight coarse step costs `orth`, a turn to
/// either side costs `diag - orth` of the entered cell (RF-coarse section 2). The fine costs 10/14
/// are fixed by the original (0x10014d00) and are not class data.
struct RouteClassCosts {
    std::uint16_t open_orth = 10;
    std::uint16_t open_diag = 13;
    std::uint16_t shore_orth = 50;
    std::uint16_t shore_diag = 69;
    /// Whether the class may enter shore-band cells at all (the original's class table mask byte of
    /// type 3: set for classes 0, 1, 2, 4, clear for class 3).
    bool shore_enterable = true;
    friend bool operator==(const RouteClassCosts&, const RouteClassCosts&) = default;
};

/// Costs per navigation class (LOGIC byte 0x10 of the ship type). The default has the classes 0..3
/// with the class-0 values; ships use class 0.
struct RouteCostTable {
    std::vector<RouteClassCosts> classes;
    /// The default table: four classes, 10/13 and 50/69, class 3 without shore.
    [[nodiscard]] static RouteCostTable original();
};

/// Empty when valid: at least one class, every cost positive, orth <= diag (a turn costs the
/// difference and must not be negative), diag - orth fits a u16 sum without overflow.
[[nodiscard]] std::string validate(const RouteCostTable& table);

// ---------------------------------------------------------------------------------------------
// Terrain
// ---------------------------------------------------------------------------------------------

/// The builder's classification of one 8 x 8-tile cell (AnnoWelt 0x100aae00 pass 1: block extrema).
enum class CellClass : std::uint8_t {
    AllWater, ///< the cell word is 0xfff
    AllLand,  ///< the cell word is 0
    Mixed,    ///< the 8 x 8 tile map is packed into a 12-bit mask
};

/// Half-open tile rectangle [x0, x1) x [z0, z1).
struct CoastRect {
    std::int32_t x0 = 0, z0 = 0, x1 = 0, z1 = 0;
    friend bool operator==(const CoastRect&, const CoastRect&) = default;
};

/// A coarse cell in world cell coordinates (tile / 8).
struct CellCoord {
    std::int32_t cx = 0, cz = 0;
    friend auto operator<=>(const CellCoord&, const CellCoord&) = default;
};

/// The terrain the route engine sees: a world of width x height tiles; a tile is water (type 8)
/// or not (land, type 9: terrain or a blocking object). Tiles outside the world are not water.
/// Implementations must be deterministic, thread-safe for concurrent reads and immutable while a
/// plan or index build reads them.
class TileSource {
  public:
    virtual ~TileSource() = default;
    [[nodiscard]] virtual std::int32_t width() const = 0;
    [[nodiscard]] virtual std::int32_t height() const = 0;
    /// Whether the tile is water. False outside [0, width) x [0, height).
    [[nodiscard]] virtual bool tile_is_water(std::int32_t x, std::int32_t z) const = 0;
    /// The builder's shortcut for the cell (cx, cz). The default scans the 64 tiles; an adapter
    /// that reproduces the original's block min/max records overrides it (the shortcut can differ
    /// from the tiles: an object whose footprint reaches into a neighbouring block).
    [[nodiscard]] virtual CellClass classify_cell(std::int32_t cx, std::int32_t cz) const;
};

/// A plain tile bitmap (1 byte per tile) for tests and tools. Starts as all water.
class TileMapSource final : public TileSource {
  public:
    TileMapSource(std::int32_t width, std::int32_t height);
    [[nodiscard]] std::int32_t width() const override { return w_; }
    [[nodiscard]] std::int32_t height() const override { return h_; }
    [[nodiscard]] bool tile_is_water(std::int32_t x, std::int32_t z) const override;
    /// Sets one tile (ignored outside the map).
    void set_water(std::int32_t x, std::int32_t z, bool water);
    /// Sets the INCLUSIVE rectangle [x0, x1] x [z0, z1] (clipped to the map).
    void fill_rect(std::int32_t x0, std::int32_t z0, std::int32_t x1, std::int32_t z1, bool water);

  private:
    std::int32_t w_ = 0, h_ = 0;
    std::vector<std::uint8_t> water_;
};

/// The cell classification by scanning the 64 tiles (the default of TileSource::classify_cell).
[[nodiscard]] CellClass classify_cell_by_tiles(const TileSource& src, std::int32_t cx,
                                               std::int32_t cz);

/// The 12-bit edge-connectivity mask of an 8 x 8 water map (row-major, index x + 8 z, non-zero =
/// water): the original packer 0x100a4130. Bit 3 s + k is set when a 4-connected water region that
/// touches source edge s also touches target edge t = (s + 3 - k) mod 4, s != t; edges 0 = E
/// (x == 7), 1 = N (z == 0), 2 = W (x == 0), 3 = S (z == 7). A region that touches one edge sets no
/// bit. (docs/re/ship-navigation.md 9.3, RF-coarse section 1.)
[[nodiscard]] std::uint16_t pack_edge_mask(std::span<const std::uint8_t, 64> water);

/// The original's discrete disc table (0x1000d6a0): half-widths hw[0..r] of the rows |dz| = 0..r.
/// Used for the goal disc in tiles (r = goal radius) and in cells (r = (radius + 4) / 8).
[[nodiscard]] std::vector<std::uint8_t> disc_half_widths(std::int32_t r);

/// The world-level coarse map (the original's [world + 0x6aa4]): 8 x 8-tile cells in 64 x 64-tile
/// sectors; a sector whose 64 cells are all open (word 0xfff, type 0) is stored as null, which is
/// also how the original represents untouched water. Class-independent: the per-class costs live in
/// RouteCostTable. A cell word is `mask | type << 16`; the type is 0 (open) or 3 (shore band).
///
/// The shore band (type 3) is an explicit input: the builder rule that creates it (pass 2 of
/// 0x100aae00, cells with a neighbour whose block range straddles level 32) was not re-executed
/// (docs/sim/ship-navigation.md section 11 item 3: a named dependency of package C). Callers pass
/// the set of band cells; the index stores nothing else about them.
///
/// The index is bound to a TileSource that must outlive it and must not change except through
/// rebuild_rect (the fine search reads live tiles from it, as the original rebuilds its fine blocks
/// from the live island tiles on every query).
class CoastIndex {
  public:
    CoastIndex() = default;
    /// Full build over the source: every cell classified, mixed cells packed, band cells typed.
    /// Cells outside the world read as land; a cell partly outside is packed with outside = land.
    CoastIndex(const TileSource& source, std::span<const CellCoord> shore_cells);

    /// Recomputes the cells that overlap the half-open tile rectangle after the source changed
    /// inside it (the original's per-rectangle rebuild when a coast or road object is placed or
    /// removed). `shore_cells` is the full band set after the change; only the cells in the
    /// rectangle take their type from it, every other cell keeps its stored word.
    void rebuild_rect(CoastRect rect, std::span<const CellCoord> shore_cells);

    /// Binds the index to another source of the same world size (a re-created adapter after the
    /// world changed). The stored words are kept: call rebuild_rect for what changed. Ignored when
    /// the size differs.
    void rebind(const TileSource& source);

    /// Empty when this index equals a clean rebuild from the bound source and `shore_cells`, else a
    /// description of the first differing cell (debug and test aid; O(world)).
    [[nodiscard]] std::string
    verify_against_clean_rebuild(std::span<const CellCoord> shore_cells) const;

    [[nodiscard]] bool valid() const { return source_ != nullptr; }
    [[nodiscard]] const TileSource& source() const { return *source_; }
    [[nodiscard]] std::int32_t width_tiles() const { return w_; }
    [[nodiscard]] std::int32_t height_tiles() const { return h_; }
    /// World size in cells, ceil(tiles / 8).
    [[nodiscard]] std::int32_t cells_x() const { return (w_ + 7) / 8; }
    [[nodiscard]] std::int32_t cells_z() const { return (h_ + 7) / 8; }
    /// The connectivity mask (bits 0..11) of the cell; 0 outside the world.
    [[nodiscard]] std::uint16_t mask(std::int32_t cx, std::int32_t cz) const;
    /// The cost type (0 or 3) of the cell; 0 outside the world.
    [[nodiscard]] std::uint16_t type(std::int32_t cx, std::int32_t cz) const;
    /// The cell word `mask | type << 16` (0 outside the world, 0xfff for a null sector).
    [[nodiscard]] std::uint32_t cell_word(std::int32_t cx, std::int32_t cz) const;
    /// Number of sectors that hold a cell array (the others are null = open water).
    [[nodiscard]] std::uint64_t allocated_sectors() const;
    /// Cell-for-cell equality of the coarse words (not of the bound source).
    [[nodiscard]] bool same_words(const CoastIndex& other) const;

  private:
    void store_word(std::int32_t cx, std::int32_t cz, std::uint32_t w);
    [[nodiscard]] std::uint32_t build_cell(std::int32_t cx, std::int32_t cz) const;

    const TileSource* source_ = nullptr;
    std::int32_t w_ = 0, h_ = 0;
    std::int32_t sectors_x_ = 0, sectors_z_ = 0;
    /// One entry per sector, row-major; empty = null sector (all cells open); else 64 words.
    std::vector<std::vector<std::uint32_t>> sectors_;
};

// ---------------------------------------------------------------------------------------------
// Query and result
// ---------------------------------------------------------------------------------------------

/// The shape of a density stamp: the original's 5 x 5 ship kernel (radius 2) or the 3 x 3 kernel
/// (radius 1) (0x100cccf4 / 0x100ccce0).
enum class StampKernel : std::uint8_t { K5, K3 };

/// One ship-density stamp: the TILE of the stamp point (the caller truncates the point, no 0.5
/// offset, no clamping; docs/re/ship-navigation.md 9.4) and its kernel. Stamps only decide among
/// goal candidates of equal standing, they never change a cost.
struct RouteStamp {
    std::int32_t tile_x = 0;
    std::int32_t tile_z = 0;
    StampKernel kernel = StampKernel::K5;
    friend bool operator==(const RouteStamp&, const RouteStamp&) = default;
};

struct RouteQuery {
    /// The truncated ship position (tile).
    std::int32_t start_x = 0;
    std::int32_t start_z = 0;
    /// The goal point in Q24.8 tile units (tile = trunc(value / 256), centre at + 0.5 tile).
    std::int32_t goal_x_q8 = 0;
    std::int32_t goal_z_q8 = 0;
    /// Navigation class (index into RouteCostTable::classes); ships use 0.
    std::uint32_t nav_class = 0;
    /// In the registry scan order the caller reproduces; only the first `unit_cap` are used.
    std::vector<RouteStamp> stamps;
};

enum class RouteStatus : std::int8_t {
    Planned = 0,         ///< a plan: `runs` is non-empty
    Arrived = 2,         ///< the chosen end tile is the start tile: no runs
    Unreachable = -2,    ///< no candidate within the radius, the learn cap, or a walled-in start
    BudgetExceeded = -3, ///< a RouteLimits work / memory / window limit; `exceeded` names it
    OutsideWindow = -4,  ///< no goal-disc cell lies inside the start-centred window
    InvalidQuery = -5,   ///< start outside the world, unknown nav_class, or invalid limits / costs
};

enum class RouteLimit : std::uint8_t { None, CoarsePops, FinePops, Memory, Window };

/// One output run: `steps` (1..run_cap) tiles in direction `dir` (0 W, 1 SW, 2 S, 3 SE, 4 E, 5 NE,
/// 6 N, 7 NW; z grows southward).
struct RouteRun {
    std::uint8_t dir = 0;
    std::uint8_t steps = 0;
    friend bool operator==(const RouteRun&, const RouteRun&) = default;
};

struct RouteWork {
    std::uint64_t coarse_pops = 0;
    std::uint64_t fine_pops = 0;
    /// Learned masks applied (= retries) when the query finished.
    std::uint32_t learned = 0;
    /// True when a retry was refused because `retry_cap` masks were already learned (the original
    /// resets its own count to 0 on that path and answers -2).
    bool retry_cap_hit = false;
    /// High-water mark of the accounted bytes.
    std::uint64_t bytes_high = 0;
    friend bool operator==(const RouteWork&, const RouteWork&) = default;
};

struct RouteResult {
    RouteStatus status = RouteStatus::Unreachable;
    RouteLimit exceeded = RouteLimit::None;
    std::vector<RouteRun> runs;
    /// The chosen goal candidate tile and its score (valid for Planned and Arrived).
    std::int32_t end_x = 0, end_z = 0;
    std::int32_t score = 0;
    /// The engine's internal `best` (callback threshold): the score of the last FREE candidate, 0
    /// while only congested candidates are stored (they never raise it).
    std::int32_t best_score = 0;
    RouteWork work;
    friend bool operator==(const RouteResult&, const RouteResult&) = default;
};

/// Plans a route. Pure: the same arguments give the same result on any thread and platform.
[[nodiscard]] RouteResult plan_route(const CoastIndex& index, const RouteQuery& query,
                                     const RouteLimits& limits, const RouteCostTable& costs);

/// The ship-density at a tile from stamps (docs/re/ship-navigation.md 9.4; kernel cell = tile -
/// (stamp tile - radius), combined in order through the original's table 0x100c15b0): 0..3.
[[nodiscard]] std::uint32_t route_density(std::span<const RouteStamp> stamps, std::int32_t tile_x,
                                          std::int32_t tile_z);

// ---------------------------------------------------------------------------------------------
// Test hooks
// ---------------------------------------------------------------------------------------------

namespace route_detail {

/// Deliberate deviations from the original, for tests only: each still produces a LEGAL route but
/// not the original's exact runs. Production code calls plan_route, which uses the default (none).
struct Perturbation {
    /// Fine neighbour order reversed (E,W,S,SE,SW,N,NE,NW instead of NW,NE,N,SW,SE,S,W,E).
    bool reverse_fine_order = false;
    /// Fine ties resolved last-in-first-out instead of FIFO.
    bool lifo_fine_ties = false;
    /// Fine diagonal cost (original 14).
    std::uint16_t fine_diag_cost = 14;
    /// Coarse ties resolved last-in-first-out.
    bool lifo_coarse_ties = false;
    /// Corner cutting allowed in the fine search (a diagonal only needs its target tile).
    bool allow_corner_cut = false;
};

[[nodiscard]] RouteResult plan_route_perturbed(const CoastIndex& index, const RouteQuery& query,
                                               const RouteLimits& limits,
                                               const RouteCostTable& costs,
                                               const Perturbation& perturbation);

/// The start-cell nibble (0x1000fa50): the start tile (cell-local lx, lz, marked even when it is
/// land) and its 4-connected water flood inside the 8 x 8 cell set one bit per touched cell edge:
/// 0x1000 W (x == 0), 0x2000 S (z == 7), 0x4000 E (x == 7), 0x8000 N (z == 0).
[[nodiscard]] std::uint16_t start_nibble(std::span<const std::uint8_t, 64> water, std::int32_t lx,
                                         std::int32_t lz);

/// The learn table 0x10013fb0 as a function: the AND mask that clears the connectivity bit of cell
/// P (arrival label `p_label`: 0x80 + dir, any other value = start cell) towards the next path cell
/// F (arrival label `f_label`, 0x80 + dir); 0xffff when the pair cannot occur.
[[nodiscard]] std::uint16_t learn_mask(std::uint32_t p_label, std::uint32_t f_label);

} // namespace route_detail

} // namespace anno::sim
