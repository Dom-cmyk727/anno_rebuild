// T33.2 pure walking inputs. Evidence: transport.md sections3.4.11-14,
// state.md section5.9. No cells/tables are state or serialized here.
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <anno/formats/annowelt.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/types.hpp>
namespace anno::sim {
using WalkTables = formats::annowelt::WalkTables;
struct WalkRecord {
    std::uint16_t code = 2, mask9 = 0;
    bool supported = true;
};
struct WalkCell {
    std::uint16_t low = 0, high = 0;
    friend bool operator==(const WalkCell&, const WalkCell&) = default;
};
[[nodiscard]] std::string validate_walk_tables(const WalkTables&);
/// FNV-1a 64 over the complete table input with explicit little-endian fields/counts.
[[nodiscard]] std::uint64_t walk_tables_digest(const WalkTables&);
/// Own tiny synthetic data, never a copy of the original tables.
[[nodiscard]] std::shared_ptr<const WalkTables> synthetic_walk_tables();
[[nodiscard]] std::uint8_t admission(const WalkTables&, std::uint32_t layer, std::uint16_t code);
[[nodiscard]] std::uint8_t cost(const WalkTables&, std::uint32_t layer, std::uint16_t code,
                                std::uint32_t j);
[[nodiscard]] std::uint16_t walk_code(const BuildingType&, std::uint32_t rotation);
[[nodiscard]] std::uint16_t walk_mask(const BuildingType&, std::uint32_t dx, std::uint32_t dy,
                                      std::uint32_t rotation);
/// The original's per-tile walk-record writer (transport.md §3.4.13). For every
/// tile (dx, dy) of footprint(type, rotation) sets walk_code =
/// walk_code(type, rotation) and walk_mask = walk_mask(type, dx, dy, rotation).
/// A type whose HEAD flags word has bit 0x10 (BuildingType::flags_word, type
/// +0x12; AnnoWelt placement test 0x10082184..0x1008218f) ORs the sampled mask
/// into the old one (0x1008239d..0x100823b0). Every other type replaces the
/// mask (0x100825c7..0x100825d9). The code is always replaced
/// (0x100825af..0x100825b7). Tiles outside the layer are skipped.
void write_walk_record(TileLayer&, const BuildingType&, std::int32_t x0, std::int32_t y0,
                       std::uint32_t rotation);
/// The original's working block (AnnoWelt 0x100820e6 and 0x10082109, step
/// 0x1008260c / 0x1008262e). The rule, not a limit: a larger footprint uses
/// more blocks.
inline constexpr std::uint32_t kPlacementBlockEdge = 8;
/// The original's placement writer (transport.md §3.4.17, AnnoWelt
/// 0x10082040..0x10082637). Blocks run row by row, then column by column;
/// inside a block, rows are outer and columns inner. The ordinary path writes
/// the id, the world dx/dy, the rotation, the phase and the anchor (1 on the
/// first written tile of the block, then 0), then the walk code and mask. The overlay
/// path (HEAD flags bit 0x10, 0x10082184..0x1008218f) writes the same fields
/// except on a tile whose id is non-zero, whose type has flag 0x10 and whose
/// anchor is set (0x100821e6..0x1008220c); the anchor flag stays set across a
/// skipped tile (0x10082289). The code is replaced and the mask is ORed on
/// every footprint tile, including one the first pass skipped
/// (0x10082342..0x100823e2). Returns how many first-dword writes that skip
/// suppressed. Byte 3 bit 6 is not stored. deviation: the height byte
/// (0x10082260, 0x10082494) is not a tile column; heights stay in Terrain
/// (OQ-060).
std::uint32_t place_object_record(TileLayer&, const TypeRegistry&, const BuildingType&,
                                  std::int32_t x0, std::int32_t y0, std::uint32_t rotation,
                                  std::uint8_t phase);
/// The clear arm of AnnoWelt 0x10082c30 (`0x10082f00..0x10082f1f`). An id of 0
/// writes nothing (`0x10082c6a`). Otherwise the origin is `(x − foot_dx, y −
/// foot_dy)` and the rotation is the tile's `foot_rotation`
/// (`0x10082ca5..0x10082cda`). Each footprint tile whose id, rotation and offset
/// match (`0x10082d98..0x10082de7`) gets id 0, `foot_dx` / `foot_dy` /
/// `foot_rotation` 0, `foot_anchor` 0 (`and al, 0x3f` at `0x10082f11` keeps
/// phase bits 0–5 and clears bit 7), code 2 and mask 0. deviation: the
/// replacement arm (the high nibble of island `+0x18`, `0x10082e43..0x10082efc`)
/// is not modelled (OQ-201). The arm also keeps the height byte and second-dword
/// bits 25–31; those are not columns (OQ-060, OQ-204, OQ-205).
void clear_object_record(TileLayer&, const TypeRegistry&, std::int32_t x, std::int32_t y);
/// The tile `remove_building` passes to `clear_object_record` (F37).
/// `holding_id` false: `(named_x, named_y)`, the coordinates RemoveAt was given
/// (AnnoWelt `0x100292d8..0x100292ef`). `holding_id` true: that tile when its
/// object id is `type.id`, otherwise the first footprint tile in
/// `place_object_record`'s block order that holds the id, otherwise the named
/// tile (no footprint tile holds it).
[[nodiscard]] std::pair<std::int32_t, std::int32_t>
removal_clear_xy(const TileLayer& layer, const BuildingType& type, std::int32_t origin_x,
                 std::int32_t origin_y, std::uint8_t rotation, std::int32_t named_x,
                 std::int32_t named_y, bool holding_id);
[[nodiscard]] WalkRecord walk_record(const Island&, const TypeRegistry&, TileXY);
/// Per-search scratch; source island, types and tables must outlive it and stay unchanged.
/// Coordinates are world tiles; rectangle clipped to island (0x10002b80).
class WalkCells {
  public:
    WalkCells(const Island&, const TypeRegistry&, const WalkTables&, TileRect,
              std::uint32_t chunk_edge = 4, std::optional<SearchArea> disc = {});
    [[nodiscard]] WalkCell get(TileXY);
    /// The mapped low word is not invertible; start floods need the raw bitmap.
    [[nodiscard]] WalkRecord record(TileXY p) const { return walk_record(island_, types_, p); }
    /// Search marks remain in this scratch store. Outside rectangle writes fail.
    bool set(TileXY, WalkCell);
    [[nodiscard]] TileRect rect() const { return rect_; }
    [[nodiscard]] bool complete() const { return complete_; }
    [[nodiscard]] std::size_t allocated_chunks() const { return chunks_.size(); }

  private:
    std::vector<WalkCell>* chunk(TileXY);
    const Island& island_;
    const TypeRegistry& types_;
    const WalkTables& tables_;
    TileRect rect_;
    std::uint32_t edge_;
    bool complete_ = true;
    std::optional<SearchArea> disc_;
    std::map<TileXY, std::vector<WalkCell>> chunks_;
};
} // namespace anno::sim
