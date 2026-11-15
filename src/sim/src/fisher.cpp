#include <algorithm>

#include <anno/sim/fisher.hpp>
#include <anno/sim/island.hpp>

namespace anno::sim {
std::uint64_t fill_fish_resources(TileLayer& tiles, std::span<const std::uint8_t> heights,
                                  TileRect area, bool enabled, std::uint8_t threshold,
                                  TypeId resource) {
    if (!enabled || heights.size() != tiles.tile_count())
        return 0;
    const auto x0 = std::max<std::int64_t>(0, std::int64_t{area.x0} - tiles.x0());
    const auto y0 = std::max<std::int64_t>(0, std::int64_t{area.y0} - tiles.y0());
    const auto x1 =
        std::min<std::int64_t>(tiles.width(), std::int64_t{area.x0} + area.w - tiles.x0());
    const auto y1 =
        std::min<std::int64_t>(tiles.height(), std::int64_t{area.y0} + area.h - tiles.y0());
    std::uint64_t count = 0;
    for (auto y = y0; y < y1; ++y) {
        for (auto x = x0; x < x1; ++x) {
            const auto i = std::uint64_t{static_cast<std::uint32_t>(y)} * tiles.width() +
                           static_cast<std::uint32_t>(x);
            if (tiles.object(i) != 0)
                continue;
            const auto cell =
                fisher_cell_at(heights, tiles.width(), tiles.height(),
                               static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
            if (!cell || cell->mean >= threshold)
                continue;
            tiles.set_object(i, resource.v);
            tiles.set_foot_dx(i, 0);
            tiles.set_foot_dy(i, 0);
            tiles.set_foot_rotation(i, 0);
            tiles.set_phase(i, 0); // low 7 original byte-3 bits; widened anchor is preserved
            tiles.set_item_height(i, cell->mean); // all other aux bits, including claim, survive
            ++count;
        }
    }
    return count;
}

std::optional<std::uint8_t> fisher_corner(std::span<const std::uint8_t> heights,
                                          std::uint32_t width, std::uint32_t height,
                                          std::uint32_t cx, std::uint32_t cy) {
    if (heights.size() != std::uint64_t{width} * height || cx > width || cy > height)
        return std::nullopt;
    if (cx == width || cy == height)
        return std::uint8_t{0};
    return heights[std::uint64_t{cy} * width + cx];
}
FisherCell fisher_cell(const std::array<std::uint8_t, 4>& corners, std::uint8_t threshold) {
    const auto mean = static_cast<std::uint8_t>(
        (std::uint32_t{corners[0]} + corners[1] + corners[2] + corners[3]) / 4);
    const bool water = mean < threshold;
    const auto blocked = static_cast<std::uint16_t>(water ? 0 : 511);
    return {mean, water, blocked, static_cast<std::uint16_t>(0x1ffU - blocked)};
}
std::optional<FisherCell> fisher_cell_at(std::span<const std::uint8_t> heights, std::uint32_t width,
                                         std::uint32_t height, std::uint32_t x, std::uint32_t y,
                                         std::uint8_t threshold) {
    if (x >= width || y >= height)
        return std::nullopt;
    const auto a = fisher_corner(heights, width, height, x, y);
    const auto b = fisher_corner(heights, width, height, x + 1, y);
    const auto c = fisher_corner(heights, width, height, x, y + 1);
    const auto d = fisher_corner(heights, width, height, x + 1, y + 1);
    if (!a || !b || !c || !d)
        return std::nullopt;
    return fisher_cell({*a, *b, *c, *d}, threshold);
}
std::optional<FisherSearchRect> fisher_search_rect(std::uint16_t x, std::uint16_t y,
                                                   std::uint32_t width, std::uint32_t height,
                                                   std::uint32_t radius) {
    if (width == 0 || height == 0)
        return std::nullopt;
    // Unsigned subtraction is modular; narrowing retains exactly the low bits,
    // including when the mathematical origin is negative. No signed overflow.
    return FisherSearchRect{
        static_cast<std::uint16_t>(std::uint64_t{x} + (width - 1) / 2 - radius),
        static_cast<std::uint16_t>(std::uint64_t{y} + (height - 1) / 2 - radius),
        static_cast<std::uint8_t>(std::uint64_t{2} * radius + 1 + (width - 1) % 2),
        static_cast<std::uint8_t>(std::uint64_t{2} * radius + 1 + (height - 1) % 2)};
}
} // namespace anno::sim
