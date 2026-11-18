#include <algorithm>
#include <limits>

#include <anno/sim/resource_import.hpp>
#include <anno/sim/site_checks.hpp>
#include <anno/sim/walk_cells.hpp>

namespace anno::sim::resource_import {
namespace {
std::uint8_t quad_mean(const VertexGrid& grid, std::int64_t x, std::int64_t y) {
    // Both callers first prove the origin quad lies on the readable grid.
    return static_cast<std::uint8_t>((std::uint32_t{*grid.at(x, y)} + *grid.at(x + 1, y) +
                                      *grid.at(x, y + 1) + *grid.at(x + 1, y + 1)) /
                                     4U);
}
// AnnoWelt 0x1007c955..0x1007cb3d: every touched world block needs a
// non-kept tile. Caller has checked bounds, resolved objects and the original
// two-column/two-row mark-layout domain. No scratch allocation per record.
bool overlay_coverage(const TileLayer& tiles, const TypeRegistry& types, std::int64_t x,
                      std::int64_t y, std::uint32_t w, std::uint32_t h) {
    const std::int64_t edge = kPlacementBlockEdge;
    for (auto by = y / edge * edge; by < y + h; by += edge)
        for (auto bx = x / edge * edge; bx < x + w; bx += edge) {
            bool marked = false;
            for (auto py = std::max(by, y); py < std::min(by + edge, y + h); ++py)
                for (auto px = std::max(bx, x); px < std::min(bx + edge, x + w); ++px) {
                    const auto i = tiles.index_of(static_cast<std::int32_t>(px),
                                                  static_cast<std::int32_t>(py));
                    const auto id = tiles.object(i);
                    const bool kept = id != 0 &&
                                      (types.find(TypeId{id})->flags_word & 0x10U) != 0 &&
                                      tiles.foot_anchor(i) != 0;
                    marked = marked || !kept;
                }
            if (!marked)
                return false;
        }
    return true;
}
// Invocation-local, id-ordered copy. No persistent registry/cache or public probe.
formats::Result<std::vector<TypeFacts>, ErrorKind> prepare(std::span<const TypeFacts> facts,
                                                           const ClimateFacts& climate) {
    if (climate.slots.size() != 16)
        return ErrorKind::Facts;
    for (const auto* row : {&climate.slots, &climate.tag104, &climate.tag201, &climate.tag404}) {
        if (row != &climate.slots && !row->empty() && row->size() != 8)
            return ErrorKind::Facts;
        if (std::any_of(row->begin(), row->end(), [](TypeId id) { return !id.valid(); }))
            return ErrorKind::Facts;
    }
    std::vector<TypeFacts> sorted(facts.begin(), facts.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const TypeFacts& a, const TypeFacts& b) { return a.id < b.id; });
    for (auto it = sorted.begin(); it != sorted.end(); ++it)
        if (!it->id.valid() || it->id.v == 0 || (it != sorted.begin() && (it - 1)->id == it->id))
            return ErrorKind::Facts;
    return sorted;
}
const TypeFacts* find(std::span<const TypeFacts> sorted, TypeId id) {
    const auto it = std::lower_bound(sorted.begin(), sorted.end(), id,
                                     [](const TypeFacts& f, TypeId key) { return f.id < key; });
    return it == sorted.end() || it->id != id ? nullptr : &*it;
}
// Only callers holding successfully prepared facts reach this selector.
formats::Result<std::uint8_t, ErrorKind> select_prepared(const TypeRegistry& types,
                                                         std::span<const TypeFacts> facts,
                                                         const ClimateFacts& climate, TypeId id,
                                                         std::uint32_t draw) {
    const auto* type = types.find(id);
    const auto* current = find(facts, id);
    if (!type || !current)
        return ErrorKind::Facts;
    TypeId candidate = id;
    auto goods = current->goods0;
    // AnnoGame 0x10001b30: distinguish kind from the content id.
    if (type->prodtype == 225 && goods >= 56 && goods <= 75) {
        --candidate.v;
    } else if (type->prodtype == 226) {
        if (id.v == std::numeric_limits<std::uint32_t>::max())
            return ErrorKind::Facts;
        const auto* next = find(facts, TypeId{id.v + 1});
        if (!next)
            return ErrorKind::Facts;
        goods = next->goods0;
    }
    if (goods == 70) {
        const std::vector<TypeId>* row = nullptr;
        switch (current->selector_tag) {
        case 104:
        case 105:
            row = &climate.tag104;
            break;
        case 201:
        case 202:
            row = &climate.tag201;
            break;
        case 404:
        case 405:
            row = &climate.tag404;
            break;
        default:
            break;
        }
        if (row) {
            if (row->size() != 8)
                return ErrorKind::Facts;
            candidate = (*row)[draw & 7U];
        }
    }
    const auto it = std::find(climate.slots.begin(), climate.slots.end(), candidate);
    return it == climate.slots.end() ? std::uint8_t{0}
                                     : static_cast<std::uint8_t>(it - climate.slots.begin());
}

} // namespace

formats::Result<std::uint8_t, ErrorKind> select(const TypeRegistry& types,
                                                std::span<const TypeFacts> facts,
                                                const ClimateFacts& climate, TypeId id,
                                                std::uint32_t draw) {
    const auto prepared = prepare(facts, climate);
    if (!prepared)
        return prepared.error();
    return select_prepared(types, prepared.value(), climate, id, draw);
}

formats::Result<Session, Error> Session::start(const TileLayer& input, const VertexGrid& grid,
                                               std::span<const std::uint8_t> variation,
                                               const TypeRegistry& types,
                                               std::span<const TypeFacts> facts,
                                               const ClimateFacts& climate) {
    if (!grid.ok() || grid.x0() != input.x0() || grid.y0() != input.y0() ||
        grid.tiles_x() != input.width() || grid.tiles_y() != input.height() ||
        variation.size() != input.tile_count() || input.width() == 0 || input.height() == 0 ||
        std::int64_t{input.x0()} + input.width() - 1 > std::numeric_limits<std::int32_t>::max() ||
        std::int64_t{input.y0()} + input.height() - 1 > std::numeric_limits<std::int32_t>::max())
        return Error{ErrorKind::Geometry, 0, 0};
    auto prepared = prepare(facts, climate);
    if (!prepared)
        return Error{prepared.error(), 0, 0};
    // Do not allow an unknown existing object to hide behind another rejection.
    for (std::uint64_t i = 0; i < input.tile_count(); ++i)
        if (input.object(i) != 0 && !types.find(TypeId{input.object(i)}))
            return Error{ErrorKind::Facts, 0, 0};
    return Session{Output{input, {variation.begin(), variation.end()}, {}, 0}, grid, types,
                   std::move(prepared.value()), climate};
}

formats::Result<Effect, Error> Session::apply(const Record& record,
                                              std::span<const std::uint32_t> draws) {
    if (failed_)
        return *failed_;
    const auto before = out_.consumed;
    const auto index = static_cast<std::uint64_t>(out_.records.size());
    const auto error = [&](ErrorKind kind) {
        failed_ = Error{kind, index, out_.consumed};
        return *failed_;
    };
    const auto* type = types_->find(record.type);
    // Object id 0 is TileLayer's empty sentinel, not a storable placement.
    if (!type || !record.type.valid() || record.type.v == 0)
        return error(ErrorKind::Facts);
    const auto group = ((type->prodtype - 1U) & ~31U) + 1U;
    // AnnoGame 0x10008931 reads EXISTING bit 0x20 in compatibility;
    // do not normalize either class's registry flags merely to share admission.
    const bool overlay_scenery =
        (type->flags_word == 0x3a || type->flags_word == 0x12 || type->flags_word == 0x8018) &&
        group == 0x141;
    if ((!overlay_scenery && type->flags_word != 0x09 && type->flags_word != 0x29) ||
        record.rotation > 3 || type->width == 0 || type->height == 0 || record.x < 0 ||
        record.y < 0 || type->width - 1 > std::numeric_limits<std::uint16_t>::max() ||
        type->height - 1 > std::numeric_limits<std::uint16_t>::max())
        return error(ErrorKind::Unsupported);
    const bool odd = (record.rotation & 1U) != 0;
    const auto w = odd ? type->height : type->width;
    const auto h = odd ? type->width : type->height;
    const site::Origin origin{record.x, record.y};
    if (!site::footprint_inside(out_.tiles, origin, w, h)) {
        out_.records.push_back(Disposition::Bounds);
        return Effect{Disposition::Bounds, 0, {}, {}};
    }
    const auto occupancy = site::footprint_occupancy(out_.tiles, *types_, *type, origin, w, h);
    if (occupancy == site::Compat::Unsupported)
        return error(ErrorKind::Facts);
    if (occupancy == site::Compat::Incompatible) {
        out_.records.push_back(Disposition::Occupied);
        return Effect{Disposition::Occupied, 0, {}, {}};
    }
    if (overlay_scenery) {
        // Original coverage mark storage is two rows with stride two. Refuse
        // inputs beyond that addressed layout, not an invented engine size cap.
        const std::int64_t edge = kPlacementBlockEdge;
        if (w > kPlacementBlockEdge || h > kPlacementBlockEdge ||
            (std::int64_t{record.x} + w - 1) / edge - record.x / edge >= 2 ||
            (std::int64_t{record.y} + h - 1) / edge - record.y / edge >= 2)
            return error(ErrorKind::Unsupported);
        if (!overlay_coverage(out_.tiles, *types_, record.x, record.y, w, h)) {
            out_.records.push_back(Disposition::Coverage);
            return Effect{Disposition::Coverage, 0, {}, {}};
        }
    }
    // AnnoGame 0x10008fa1: ordinary mode 1 reads only the origin quad.
    // Overlay modes share quantized cell minima: mode 0 selects their high,
    // mode 2 their low (0x10009048..0x10009109). No raw file height is used.
    const std::int64_t x = record.x, y = record.y;
    auto height = quad_mean(grid_, x, y);
    if (overlay_scenery) {
        std::uint8_t lo = 255, hi = 0;
        for (std::uint32_t dy = 0; dy < h; ++dy)
            for (std::uint32_t dx = 0; dx < w; ++dx) {
                const auto px = x + dx, py = y + dy;
                const auto minimum = std::min({*grid_.at(px, py), *grid_.at(px + 1, py),
                                               *grid_.at(px, py + 1), *grid_.at(px + 1, py + 1)});
                const auto quantized = static_cast<std::uint8_t>(minimum & 0xf0U);
                lo = std::min(lo, quantized);
                hi = std::max(hi, quantized);
            }
        height = (type->flags_word & 3U) == 0 ? hi : lo;
    }
    // AnnoWelt 0x1007cbdf: group 0x141 bypasses the final ordinary level gate.
    if (height < 32 && group != 0x141) {
        out_.records.push_back(Disposition::Terrain);
        return Effect{Disposition::Terrain, 0, {}, height};
    }
    // AnnoWelt 0x10089417: commit before the resource draw and selector.
    place_object_record(out_.tiles, *types_, *type, record.x, record.y, record.rotation,
                        static_cast<std::uint8_t>(record.raw_state & 63U), height);
    std::optional<std::uint8_t> chosen;
    if (type->prodtype >= 225 && type->prodtype <= 256) {
        if (draws.empty())
            return error(ErrorKind::Exhausted);
        const auto draw = draws.front();
        ++out_.consumed;
        const auto variant = select_prepared(*types_, facts_, climate_, record.type, draw);
        if (!variant)
            return error(variant.error());
        chosen = variant.value();
        const auto i = out_.tiles.index_of(record.x, record.y);
        // Separate island +0x18 plane; do not alias phase or aux.
        out_.variation[i] = static_cast<std::uint8_t>((out_.variation[i] & 15U) |
                                                      (std::uint32_t{variant.value()} << 4U));
    }
    out_.records.push_back(Disposition::Accepted);
    return Effect{Disposition::Accepted, out_.consumed - before, chosen, height};
}

formats::Result<std::uint64_t, Error> Session::close_import_block(std::int32_t x, std::int32_t y,
                                                                  std::uint32_t w, std::uint32_t h,
                                                                  std::uint8_t threshold) {
    if (failed_)
        return *failed_;
    const auto left = std::max(std::int64_t{x}, std::int64_t{out_.tiles.x0()});
    const auto top = std::max(std::int64_t{y}, std::int64_t{out_.tiles.y0()});
    const auto right =
        std::min(std::int64_t{x} + w, std::int64_t{out_.tiles.x0()} + out_.tiles.width());
    const auto bottom =
        std::min(std::int64_t{y} + h, std::int64_t{out_.tiles.y0()} + out_.tiles.height());
    std::uint64_t changed = 0;
    for (auto py = top; py < bottom; ++py)
        for (auto px = left; px < right; ++px) {
            if (quad_mean(grid_, px, py) >= threshold)
                continue;
            const auto i =
                out_.tiles.index_of(static_cast<std::int32_t>(px), static_cast<std::int32_t>(py));
            const auto before = out_.tiles.walk_mask(i);
            const auto after = static_cast<std::uint16_t>(before | 0x1ffU);
            out_.tiles.set_walk_mask(i, after);
            changed += before != after;
        }
    return changed;
}

formats::Result<Output, Error> Session::finish() {
    if (failed_)
        return *failed_;
    failed_ =
        Error{ErrorKind::Facts, static_cast<std::uint64_t>(out_.records.size()), out_.consumed};
    return std::move(out_);
}
Output Session::diagnostic() && {
    if (!failed_)
        failed_ =
            Error{ErrorKind::Facts, static_cast<std::uint64_t>(out_.records.size()), out_.consumed};
    return std::move(out_);
}

formats::Result<Output, Error>
evaluate(const TileLayer& input, const VertexGrid& grid, std::span<const std::uint8_t> variation,
         const TypeRegistry& types, std::span<const TypeFacts> facts, const ClimateFacts& climate,
         std::span<const Record> records, std::span<const std::uint32_t> draws) {
    auto started = Session::start(input, grid, variation, types, facts, climate);
    if (!started)
        return started.error();
    auto& session = started.value();
    std::uint64_t used = 0;
    for (const auto& record : records) {
        const auto result = session.apply(record, draws.subspan(static_cast<std::size_t>(used)));
        if (!result)
            return result.error();
        used += result.value().draws;
    }
    return session.finish();
}
} // namespace anno::sim::resource_import
