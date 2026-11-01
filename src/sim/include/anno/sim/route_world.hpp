// The route engine's view of the real terrain (MAP-NAV-2 package C, part c): a TileSource over a
// World's islands in island-slot order, with the original's land rules.
//
//  * Terrain: a tile is water iff floor((v00 + v10 + v01 + v11) / 4) <= 19 over its four corner
//    vertices (AnnoWelt 0x10090350; threshold ftol(0.78 / 0.04) = 19). The vertex grid is the
//    repo's VertexGrid: column W and rows H, H + 1 read the zero padding, so the tiles of an
//    island's last column and row pull their average down (docs/re/ship-navigation.md 9.2).
//  * Objects: every tile that carries an object blocks iff ((h + base) & 0xff) > 19, with h =
//    trunc(f32 HEAD[0x14] * 25) computed exactly and base the placed item's height byte (rec[4]).
//    There is no class or flag test: not every footprint is land, a footprint is not exempt.
//  * Outside every island rectangle (open sea) a tile is water; outside the world it is not.
//  * Cell classification reproduces the builder's block shortcut (0x100aae00 pass 1, block records
//  of
//    0x10084190: min / max of the 9 x 9 vertices, the max raised by the anchor tile's object top
//    (trunc(f32 / 0.04f) + base) & 0xff, both halved) for cells that are exactly one block of an
//    island whose origin is 8-aligned and whose width and height are multiples of 8. Any other cell
//    (open sea, island borders that do not tile into blocks, several islands) is classified by its
//    tiles. The same records (block_range) give the shore band (type 3) that CoastIndex derives,
//    pass 2 of the builder; islands that are not aligned blocks have no records and derive no band.
//
// Documented limits of this adapter (nothing is guessed): the 11 object types that carry a height
// map in the original (1020, 1021, 2701-2708, 2740) use their scalar here, and the sim does not
// carry the placed item's height byte (TileLayer::aux is a stand-in that every loader leaves 0), so
// base is 0 unless the caller supplies RouteTerrainOptions::item_base.
//
// The source reads the World; the World must not change while a CoastIndex built on it is planning.
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include <anno/sim/island.hpp>
#include <anno/sim/route.hpp>
#include <anno/sim/terrain_view.hpp>
#include <anno/sim/world.hpp>

namespace anno::sim {

struct RouteTerrainOptions {
    /// The placed item's height byte (rec[4]) of the object tile with this row-major tile index, or
    /// null for 0 everywhere.
    std::function<std::uint8_t(const Island&, std::uint64_t tile_index)> item_base;
    /// Objects block per the predicate above; false ignores them (terrain only).
    bool objects_block = true;
    /// Use the builder's block min / max shortcut for aligned island blocks; false classifies every
    /// cell by its tiles.
    bool block_shortcut = true;
};

/// trunc(f * 25) for the f32 with this bit pattern, exact (the object type's height scalar,
/// AnnoGame 0x10007f40 without a height map). Total: infinities and NaN give 0.
[[nodiscard]] std::int64_t object_height_units(std::uint32_t f32_bits);

/// trunc(f / 0.04f) for the f32 with this bit pattern, exact (the object top of the block records,
/// 0x10084190). Total.
[[nodiscard]] std::int64_t object_top_units(std::uint32_t f32_bits);

/// The blocking predicate of an object tile: ((h + base) & 0xff) > 19 (an 8-bit unsigned add: a
/// negative h wraps and blocks).
[[nodiscard]] bool object_tile_blocks(std::int64_t h, std::uint32_t base);

class WorldTileSource final : public TileSource {
  public:
    /// The islands of a world in slot order, its type registry and its settings' width and height.
    explicit WorldTileSource(const World& world, RouteTerrainOptions options = {});
    /// Explicit form (tests and tools): islands in the slot order given, the registry the object
    /// ids resolve in, and the world size in tiles.
    WorldTileSource(std::span<const Island* const> islands, const TypeRegistry& types,
                    std::int32_t width, std::int32_t height, RouteTerrainOptions options = {});

    [[nodiscard]] std::int32_t width() const override { return w_; }
    [[nodiscard]] std::int32_t height() const override { return h_; }
    [[nodiscard]] bool tile_is_water(std::int32_t x, std::int32_t z) const override;
    [[nodiscard]] CellClass classify_cell(std::int32_t cx, std::int32_t cz) const override;

    /// The block record of an aligned island block (min and max halved, the max raised by the
    /// anchor object tops): the record classify_cell tests and the shore band is derived from.
    [[nodiscard]] bool block_range(std::int32_t cx, std::int32_t cz, std::uint8_t& min_half,
                                   std::uint8_t& max_half) const override;
    /// Whether classify_cell answers this cell from the block shortcut (else from its tiles).
    [[nodiscard]] bool cell_uses_shortcut(std::int32_t cx, std::int32_t cz) const;

  private:
    struct Entry {
        const Island* island = nullptr;
        VertexGrid grid;
        bool aligned = false; ///< origin and size are multiples of 8
    };
    void init(std::span<const Island* const> islands, std::int32_t width, std::int32_t height);
    [[nodiscard]] const Entry* entry_at(std::int32_t x, std::int32_t z) const;
    [[nodiscard]] bool object_blocks(const Entry& e, std::int32_t x, std::int32_t z) const;
    [[nodiscard]] std::uint32_t head_f32(std::uint32_t object) const;

    const TypeRegistry* types_;
    RouteTerrainOptions opt_;
    std::int32_t w_ = 0, h_ = 0;
    std::vector<Entry> entries_; ///< island-slot order
    /// For each world cell (row-major, cells_x wide): index into entries_ of the island whose
    /// rectangle contains the whole cell, or -1.
    std::vector<std::int32_t> cell_entry_;
    std::int32_t cells_x_ = 0, cells_z_ = 0;
};

} // namespace anno::sim
