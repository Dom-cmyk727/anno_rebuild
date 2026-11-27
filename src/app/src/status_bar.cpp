#include <algorithm>
#include <array>
#include <utility>

#include <anno/app/status_bar.hpp>

namespace anno::app {

namespace {

bool holds(const TerritoryIsland& is, std::int64_t x, std::int64_t y) {
    return x >= is.x0 && y >= is.y0 && x < std::int64_t{is.x0} + is.w &&
           y < std::int64_t{is.y0} + is.h;
}

/// The row-major tile index of (x, y) on `is` (the caller checked `holds`).
std::size_t tile_index(const TerritoryIsland& is, std::int64_t x, std::int64_t y) {
    return static_cast<std::size_t>((y - is.y0) * std::int64_t{is.w} + (x - is.x0));
}

/// The city of `is` whose territory holds (x, y): the tile's owner value is a city number
/// (island slot +0xec, AnnoWelt 0x100860c0: the owner nibble), neutral_owner is none.
const TerritoryCity* city_at(const TerritoryView& view, const TerritoryIsland& is, std::int64_t x,
                             std::int64_t y) {
    if (!holds(is, x, y)) {
        return nullptr;
    }
    const std::size_t i = tile_index(is, x, y);
    if (i >= is.owner.size() || is.owner[i] == view.neutral_owner) {
        return nullptr;
    }
    for (const TerritoryCity& c : is.cities) {
        if (c.number == is.owner[i]) {
            return &c;
        }
    }
    return nullptr;
}

bool contains(const std::vector<std::uint32_t>& sorted, std::uint32_t v) {
    return std::binary_search(sorted.begin(), sorted.end(), v);
}

} // namespace

std::vector<std::uint32_t> fertility_segments(std::uint32_t climate,
                                              const std::vector<std::uint32_t>& climate_mask,
                                              const StatusBarRule& rule) {
    std::vector<std::uint32_t> out;
    if (climate >= climate_mask.size()) {
        return out;
    }
    const std::uint32_t mask = climate_mask[climate];
    for (std::uint32_t i = 0; i < rule.segment_count && i < 32 && out.size() < rule.slots; ++i) {
        const std::uint32_t bit = std::uint32_t{1} << i;
        if ((mask & bit) != 0 && (rule.always_set_bits & bit) == 0 && i != rule.skipped_segment) {
            out.push_back(i);
        }
    }
    return out;
}

StatusPick pick_status(const TerritoryView& view, std::optional<sim::TileXY> tile,
                       std::uint32_t local_player, std::optional<sim::TileRect> armed,
                       const StatusBarRule& rule) {
    StatusPick out;
    out.tile = tile;
    if (!tile) {
        return out; // gold only, the row hidden
    }
    const std::int64_t x = tile->x, y = tile->y;
    const TerritoryIsland* island = nullptr;
    for (const TerritoryIsland& is : view.islands) {
        if (holds(is, x, y)) {
            island = &is;
            break;
        }
    }
    if (island == nullptr) {
        return out; // open sea: gold only (0x10009920), the row hidden at once (0x100097c9)
    }
    out.island = island->index;

    // The counters (0x10009920).
    if (const TerritoryCity* c = city_at(view, *island, x, y)) {
        out.city = sim::CityRef{sim::IslandId{island->index}, sim::CityNo{c->number}};
        if (c->owner == local_player) {
            out.counters = StatusCounters::MaterialsResidents;
            out.counters_city = out.city;
        } else if (contains(c->known_by, local_player)) {
            out.counters = StatusCounters::Residents;
            out.counters_city = out.city;
        }
    } else if (armed && armed->w > 0 && armed->h > 0) {
        // No city at the tile, a building armed: its corners in the original's order (jump table
        // 0x10009ca4: (x0, y0), (x0, y1 - 1), (x1 - 1, y0), (x1 - 1, y1 - 1)), on the same
        // island; the first in a city of the local player gives that city.
        const std::int64_t x0 = armed->x0, y0 = armed->y0;
        const std::int64_t x1 = x0 + armed->w, y1 = y0 + armed->h;
        const std::array<std::pair<std::int64_t, std::int64_t>, 4> corners{
            {{x0, y0}, {x0, y1 - 1}, {x1 - 1, y0}, {x1 - 1, y1 - 1}}};
        for (const auto& [cx, cy] : corners) {
            const TerritoryCity* corner = city_at(view, *island, cx, cy);
            if (corner != nullptr && corner->owner == local_player) {
                out.counters = StatusCounters::MaterialsResidents;
                out.counters_city =
                    sim::CityRef{sim::IslandId{island->index}, sim::CityNo{corner->number}};
                break;
            }
        }
    }

    // The fertility row (0x10009770): the discovery bit first, then land or sea.
    if (!contains(island->discovered_by, local_player)) {
        return out; // HideNow
    }
    bool land = true; // no height column: the whole rectangle counts as land (TerritoryIsland)
    if (island->land) {
        const std::size_t i = tile_index(*island, x, y);
        land = i < island->land->size() && (*island->land)[i] != 0;
    }
    if (!land) {
        out.fertility = FertilityAction::HideLater;
        return out;
    }
    out.segments = fertility_segments(island->climate, view.climate_mask, rule);
    out.fertility = out.segments.empty() ? FertilityAction::HideLater : FertilityAction::Show;
    return out;
}

void FertilityRow::update(const StatusPick& pick, std::uint64_t now_ms, const StatusBarRule& rule) {
    switch (pick.fertility) {
    case FertilityAction::Show:
        shown_ = pick.segments;
        hide_at_.reset();
        break;
    case FertilityAction::HideNow:
        shown_.clear();
        hide_at_.reset();
        break;
    case FertilityAction::HideLater:
        if (shown_.empty()) {
            hide_at_.reset(); // nothing to hide: the original asks again only on a new tile
            break;
        }
        if (!hide_at_) {
            hide_at_ = now_ms + rule.hide_delay_ms;
        }
        if (now_ms >= *hide_at_) {
            shown_.clear();
            hide_at_.reset();
        }
        break;
    }
}

std::string status_counters_name(StatusCounters c) {
    switch (c) {
    case StatusCounters::Gold:
        return "gold";
    case StatusCounters::Residents:
        return "residents";
    case StatusCounters::MaterialsResidents:
        return "materials+residents";
    }
    return "gold";
}

std::string fertility_text(const std::vector<std::uint32_t>& shown) {
    if (shown.empty()) {
        return "hidden";
    }
    std::string out;
    for (const std::uint32_t s : shown) {
        if (!out.empty()) {
            out += ',';
        }
        out += std::to_string(s);
    }
    return out;
}

std::string status_tile_text(std::optional<sim::TileXY> tile) {
    return tile ? std::to_string(tile->x) + "," + std::to_string(tile->y) : std::string("none");
}

std::string status_island_text(const StatusPick& pick) {
    if (!pick.tile) {
        return "none";
    }
    return pick.island ? std::to_string(*pick.island) : std::string("sea");
}

std::string status_city_text(std::optional<sim::CityRef> city) {
    return city ? std::to_string(city->island.v) + ":" + std::to_string(city->city.v)
                : std::string("none");
}

} // namespace anno::app
