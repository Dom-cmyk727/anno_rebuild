// The zoom levels of a scene: the list of tile sizes the picture can be built
// at (docs/render/iso.md "Continuous zoom", stage 3 design; PLAN 0.1: the
// groups are data, not a constant).
//
// The three groups of the original are the built levels; derived levels
// (stage 3) are added after them, each the previous level reduced 2x. A tile
// size is w / 2^shift by h / 2^shift: integers, so stand-in ratios between
// any two levels stay exact (num / den below) and no pixel loop sees a
// fraction. Scene content (Anlagen layouts, sea patterns, river lattices) is
// built per built level only; iso::kTile / kGroups remain the table of those.
#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <anno/render/iso.hpp>

namespace anno::render {

struct ZoomLevel {
    /// Tile width and height (screen pixels of the diamond's bounding box) of
    /// the level, times 2^shift.
    int w = 0, h = 0;
    /// 0 for a built level; k for a level reduced 2^k times from a built one.
    unsigned shift = 0;
    /// Derived from `source` (a level index) instead of built from the files.
    bool derived = false;
    std::size_t source = 0;
};

class ZoomLevels {
  public:
    /// The original three groups, built (kTile).
    static const ZoomLevels& original() {
        static const ZoomLevels levels = [] {
            ZoomLevels l;
            for (const TileSize& t : kTile) {
                l.levels_.push_back({t.w, t.h, 0, false, 0});
            }
            return l;
        }();
        return levels;
    }

    [[nodiscard]] std::size_t size() const { return levels_.size(); }
    [[nodiscard]] const ZoomLevel& operator[](std::size_t i) const { return levels_[i]; }
    /// Levels built from the original files (they come first).
    [[nodiscard]] std::size_t built_count() const {
        std::size_t n = 0;
        while (n < levels_.size() && !levels_[n].derived) {
            ++n;
        }
        return n;
    }

    /// Tile width / height of level `i` in screen pixels (exact for built
    /// levels, a power-of-two fraction for derived ones).
    [[nodiscard]] double tile_w(std::size_t i) const { return scaled(levels_[i].w, levels_[i]); }
    [[nodiscard]] double tile_h(std::size_t i) const { return scaled(levels_[i].h, levels_[i]); }

    /// tile(from) / tile(to) as an exact fraction (num, den) per axis: the scale
    /// of a pixel of `to` into a pixel of `from`. (84, 42) -> 2/1; with shifts,
    /// (w_from * 2^shift_to) / (w_to * 2^shift_from).
    [[nodiscard]] std::pair<int, int> ratio_w(std::size_t from, std::size_t to) const {
        return {levels_[from].w << levels_[to].shift, levels_[to].w << levels_[from].shift};
    }
    [[nodiscard]] std::pair<int, int> ratio_h(std::size_t from, std::size_t to) const {
        return {levels_[from].h << levels_[to].shift, levels_[to].h << levels_[from].shift};
    }

    /// Appends a level (the derived levels of stage 3 use this).
    void push_back(const ZoomLevel& level) { levels_.push_back(level); }

    /// The built level that level `i` descends from (itself when built).
    [[nodiscard]] std::size_t ancestor(std::size_t i) const {
        while (i < levels_.size() && levels_[i].derived) {
            i = levels_[i].source;
        }
        return i;
    }

    /// The original levels plus `derived` levels, each the previous one reduced
    /// 2x (tile size halved), after the last built level.
    [[nodiscard]] static ZoomLevels with_derived(std::size_t derived) {
        ZoomLevels l = original();
        const std::size_t last = l.levels_.size() - 1;
        for (std::size_t k = 1; k <= derived; ++k) {
            l.levels_.push_back({l.levels_[last].w, l.levels_[last].h, static_cast<unsigned>(k),
                                 true, l.levels_.size() - 1});
        }
        return l;
    }

    /// The number of derived levels after which a scene of `scene_w` x `scene_h`
    /// pixels at the last built level fits a window of `win_w` x `win_h` pixels
    /// (0 if it already fits there), at most `max_derived`.
    [[nodiscard]] static std::size_t derived_to_fit(double scene_w, double scene_h, double win_w,
                                                    double win_h, std::size_t max_derived) {
        std::size_t k = 0;
        while (k < max_derived && (scene_w > win_w || scene_h > win_h)) {
            scene_w /= 2.0;
            scene_h /= 2.0;
            ++k;
        }
        return k;
    }

    /// `h_num / h_den` HEIGHT units of lift in pixels at level `i` (Lift::scaled
    /// of the built ancestor, divided by the reduction).
    [[nodiscard]] int lift_scaled(const Lift& lift, std::int64_t h_num, std::int64_t h_den,
                                  std::size_t i) const {
        return lift.scaled(h_num, h_den << levels_[i].shift, ancestor(i));
    }

  private:
    static double scaled(int v, const ZoomLevel& l) {
        return static_cast<double>(v) / static_cast<double>(std::uint64_t{1} << l.shift);
    }
    std::vector<ZoomLevel> levels_;
};

} // namespace anno::render
