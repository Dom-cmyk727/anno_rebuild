#include <anno/sim/fisher.hpp>

namespace anno::sim {
std::optional<std::uint8_t> fisher_corner(std::span<const std::uint8_t> heights,
                                          std::uint32_t width, std::uint32_t height,
                                          std::uint32_t cx, std::uint32_t cy) {
    if (heights.size() != std::uint64_t{width} * height || cx > width || cy > height)
        return std::nullopt;
    if (cx == width || cy == height)
        return 0;
    return heights[std::uint64_t{cy} * width + cx];
}
FisherCell fisher_cell(const std::array<std::uint8_t, 4>& corners, std::uint8_t threshold) {
    const auto mean = static_cast<std::uint8_t>(
        (std::uint32_t{corners[0]} + corners[1] + corners[2] + corners[3]) / 4);
    const bool water = mean < threshold;
    return {mean, water, static_cast<std::uint16_t>(water ? 0 : 511)};
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
