// State-free fisher geometry; docs/re/fisher.md §§3/5. No scheduler wiring.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace anno::sim {
// AnnoWelt.dll 0x100e34dc, initialized at 0x1008fea0/0x1008ff00:
// byte(trunc(value / f32(0.04))) = 32. A rule value, not a size limit;
// passed explicitly (or by this default) so future rules can select another value.
inline constexpr std::uint8_t kFisherWaterThreshold = 32;

// Logical corner (cx,cy), 0 <= cx <= width, 0 <= cy <= height. The supplied
// row-major HEIGHT column must have exactly width*height bytes. Last row/column
// are zero (0x1008c18a–0x1008c1d3, 0x10084870). No allocation or stored grid.
[[nodiscard]] std::optional<std::uint8_t> fisher_corner(std::span<const std::uint8_t> heights,
                                                        std::uint32_t width, std::uint32_t height,
                                                        std::uint32_t cx, std::uint32_t cy);

struct FisherCell {
    std::uint8_t mean;
    bool walkable_water;
    std::uint16_t goal_mask;
    friend bool operator==(const FisherCell&, const FisherCell&) = default;
};
// Four corners are geometry, not a fixed content count. AnnoWelt.dll
// 0x10090560–0x100906df / 0x1009226b–0x100922df: unsigned mean, strict water
// comparison, opposite goal polarity. Equality is a goal and closed to walking.
[[nodiscard]] FisherCell fisher_cell(const std::array<std::uint8_t, 4>& corners,
                                     std::uint8_t threshold = kFisherWaterThreshold);
[[nodiscard]] std::optional<FisherCell>
fisher_cell_at(std::span<const std::uint8_t> heights, std::uint32_t width, std::uint32_t height,
               std::uint32_t x, std::uint32_t y, std::uint8_t threshold = kFisherWaterThreshold);

struct FisherSearchRect {
    std::uint16_t x, y;
    std::uint8_t width, height;
    friend bool operator==(const FisherSearchRect&, const FisherSearchRect&) = default;
};
// AnnoWelt.dll 0x10059210–0x10059398; fisher_ref.py search_rect. Integer-only.
// Origins wrap modulo 65536, extents modulo 256 (including zero), never clamp
// to the map. These narrow result widths reproduce the decoded arithmetic,
// not a map/radius limit. Widened intermediates allow any u32 radius/footprint.
// A zero footprint is invalid; no rectangle is returned. No float overload.
[[nodiscard]] std::optional<FisherSearchRect> fisher_search_rect(std::uint16_t x, std::uint16_t y,
                                                                 std::uint32_t width,
                                                                 std::uint32_t height,
                                                                 std::uint32_t radius);
} // namespace anno::sim
