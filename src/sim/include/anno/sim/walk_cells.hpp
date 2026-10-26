// T33.2 pure walking inputs. Evidence: transport.md sections3.4.11-14,
// state.md section5.9. No cells/tables are state or serialized here.
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
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
