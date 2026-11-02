// The corner-height grid of an island, as a checked derived view (task MAP-2 U1;
// research: docs/re/ship-founding.md section 12.3, contract: docs/sim/ship-founding.md).
//
// Terrain::height holds the HEIGHT records of the island file: W x H bytes, row-major,
// one byte per TILE. The original keeps those samples in a corner grid of stride W + 1
// (AnnoWelt.dll island +0x14; allocated (H + 2) * (W + 1) bytes, zero-filled at
// 0x1008c18a-0x1008c1d0; the HEIGHT block copy 0x10084870 writes sample (x, y) to
// vertex (x, y) and never touches column W or rows H and H + 1). The placement
// predicates read that grid (AnnoWelt 0x1007c790 through the type's slot +0x68), so
// the vertex (x0 + W, y) and (x, y0 + H) of the footprint's window read 0.
//
// This view is derived, never stored: it points into the island's immutable shared
// Terrain, allocates nothing, copies nothing and is valid while the Island (or the
// Terrain it shares) is alive and unmodified. It changes no state, no serialization and
// no hash. Vertex coordinates are WORLD tile coordinates: vertex (vx, vy) is the
// north-west corner of tile (vx, vy); the valid range is x0 <= vx <= x0 + W and
// y0 <= vy <= y0 + H (the south and east borders of the last tile).
#pragma once

#include <cstdint>
#include <optional>

#include <anno/sim/island.hpp>

namespace anno::sim {

/// Why a view cannot answer.
enum class VertexGridStatus : std::uint8_t {
    /// The island has terrain and a HEIGHT column of exactly W x H bytes.
    Ok,
    /// Island::terrain is null.
    NoTerrain,
    /// The columns are absent but the digest is not the digest of absent columns: the
    /// placeholder a world read from the flat state stream holds (island.hpp, Terrain).
    HashOnlyPlaceholder,
    /// The terrain exists and has no HEIGHT column (a scenario without HEIGHT records,
    /// OQ-060 shape).
    HeightColumnAbsent,
    /// The HEIGHT column is present but its size is not W x H.
    HeightColumnWrongSize,
};

/// The height of a vertex that the original's grid holds as zero (column W, rows H and
/// H + 1; docs/re/ship-founding.md section 12.3).
inline constexpr std::uint8_t kBoundaryVertexHeight = 0;

class VertexGrid {
  public:
    /// O(1): no allocation, no scan of the column.
    [[nodiscard]] static VertexGrid of(const Island& island);

    [[nodiscard]] VertexGridStatus status() const { return status_; }
    [[nodiscard]] bool ok() const { return status_ == VertexGridStatus::Ok; }

    /// The island's origin and size in tiles (the view's coordinate space).
    [[nodiscard]] std::int32_t x0() const { return x0_; }
    [[nodiscard]] std::int32_t y0() const { return y0_; }
    [[nodiscard]] std::uint32_t tiles_x() const { return w_; }
    [[nodiscard]] std::uint32_t tiles_y() const { return h_; }
    /// The original grid's row stride, W + 1 (a u64: W is a u32).
    [[nodiscard]] std::uint64_t stride() const { return std::uint64_t{w_} + 1; }

    /// Whether the vertex lies on the grid's reachable range: x0 <= vx <= x0 + W and
    /// y0 <= vy <= y0 + H. Independent of the status.
    [[nodiscard]] bool contains(std::int64_t vx, std::int64_t vy) const;

    /// The height byte of the vertex (world tile coordinates), or nullopt when the
    /// status is not Ok or the vertex is outside the range. Total: never reads outside
    /// the HEIGHT column. Column W and row H read kBoundaryVertexHeight.
    [[nodiscard]] std::optional<std::uint8_t> at(std::int64_t vx, std::int64_t vy) const;

    /// The index the original uses into its runtime grid for the vertex,
    /// (vy - y0) * (W + 1) + (vx - x0), or nullopt outside the range. Provided for
    /// comparison with the original's arithmetic, not for indexing Terrain::height.
    [[nodiscard]] std::optional<std::uint64_t> runtime_index(std::int64_t vx,
                                                             std::int64_t vy) const;

  private:
    VertexGridStatus status_ = VertexGridStatus::NoTerrain;
    std::int32_t x0_ = 0, y0_ = 0;
    std::uint32_t w_ = 0, h_ = 0;
    const std::uint8_t* height_ = nullptr;
};

} // namespace anno::sim
