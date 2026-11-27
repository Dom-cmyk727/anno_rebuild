#include <algorithm>

#include <anno/app/range_view.hpp>

namespace anno::app {
namespace {

bool listed(const std::vector<std::uint32_t>& v, std::uint32_t p) {
    return std::find(v.begin(), v.end(), p) != v.end();
}

std::optional<RangeCity> city_at(const TerritoryView& view, const std::optional<sim::TileXY>& t) {
    if (!t) {
        return std::nullopt;
    }
    const auto c = range_owner_at(view, *t);
    if (!c || c->owner == view.neutral_owner) {
        return std::nullopt;
    }
    return c;
}

} // namespace

std::optional<RangeCity> range_owner_at(const TerritoryView& view, sim::TileXY t) {
    for (const TerritoryIsland& is : view.islands) {
        const std::int64_t dx = std::int64_t{t.x} - is.x0;
        const std::int64_t dy = std::int64_t{t.y} - is.y0;
        if (dx < 0 || dy < 0 || dx >= std::int64_t{is.w} || dy >= std::int64_t{is.h}) {
            continue;
        }
        const auto i = static_cast<std::size_t>(dy) * is.w + static_cast<std::size_t>(dx);
        if (i >= is.owner.size()) {
            return std::nullopt;
        }
        return RangeCity{is.index, is.owner[i]};
    }
    return std::nullopt;
}

std::optional<RangeCity> range_city(const RangeInput& in, const RangeTriggers& trig,
                                    const TerritoryView& view) {
    const bool armed = (in.armed_road && trig.armed_road) ||
                       (in.armed_prodtype && listed(trig.armed_prodtypes, *in.armed_prodtype));
    if (armed) {
        return city_at(view, in.pointer_tile);
    }
    if (in.selected_prodtype && listed(trig.selected_prodtypes, *in.selected_prodtype)) {
        return city_at(view, in.selected_tile);
    }
    return std::nullopt;
}

bool range_dark(const TerritoryView& view, std::size_t island_slot, std::size_t i,
                const RangeCity& city) {
    const TerritoryIsland& is = view.islands.at(island_slot);
    return is.index != city.island || i >= is.owner.size() || is.owner[i] != city.owner;
}

std::size_t range_dark_count(const TerritoryView& view, const RangeCity& city) {
    std::size_t n = 0;
    for (std::size_t s = 0; s < view.islands.size(); ++s) {
        const std::size_t tiles = std::size_t{view.islands[s].w} * view.islands[s].h;
        for (std::size_t i = 0; i < tiles; ++i) {
            n += range_dark(view, s, i, city) ? 1 : 0;
        }
    }
    return n;
}

} // namespace anno::app
