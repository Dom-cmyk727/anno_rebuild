// A screen rectangle in pixels.
#pragma once

#include <algorithm>
#include <optional>

namespace anno::render {

/// `x`, `y` = top-left corner.
struct Rect {
    int x = 0, y = 0, w = 0, h = 0;

    /// Overlap of two rectangles, nullopt if they do not overlap.
    [[nodiscard]] std::optional<Rect> intersect(const Rect& o) const {
        const int x0 = std::max(x, o.x), y0 = std::max(y, o.y);
        const int x1 = std::min(x + w, o.x + o.w), y1 = std::min(y + h, o.y + o.h);
        if (x1 > x0 && y1 > y0) {
            return Rect{x0, y0, x1 - x0, y1 - y0};
        }
        return std::nullopt;
    }
    /// Smallest rectangle containing both.
    [[nodiscard]] Rect unite(const Rect& o) const {
        const int x0 = std::min(x, o.x), y0 = std::min(y, o.y);
        const int x1 = std::max(x + w, o.x + o.w), y1 = std::max(y + h, o.y + o.h);
        return Rect{x0, y0, x1 - x0, y1 - y0};
    }
    bool operator==(const Rect&) const = default;
};

} // namespace anno::render
