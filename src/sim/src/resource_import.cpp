#include <algorithm>
#include <limits>

#include <anno/sim/resource_import.hpp>
#include <anno/sim/site_checks.hpp>
#include <anno/sim/walk_cells.hpp>

namespace anno::sim::resource_import {
namespace {
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

formats::Result<Output, Error>
evaluate(const TileLayer& input, const VertexGrid& grid, std::span<const std::uint8_t> variation,
         const TypeRegistry& types, std::span<const TypeFacts> facts, const ClimateFacts& climate,
         std::span<const Record> records, std::span<const std::uint32_t> draws) {
    if (!grid.ok() || grid.x0() != input.x0() || grid.y0() != input.y0() ||
        grid.tiles_x() != input.width() || grid.tiles_y() != input.height() ||
        variation.size() != input.tile_count() || input.width() == 0 || input.height() == 0 ||
        std::int64_t{input.x0()} + input.width() - 1 > std::numeric_limits<std::int32_t>::max() ||
        std::int64_t{input.y0()} + input.height() - 1 > std::numeric_limits<std::int32_t>::max())
        return Error{ErrorKind::Geometry, 0, 0};
    const auto prepared = prepare(facts, climate);
    if (!prepared)
        return Error{prepared.error(), 0, 0};
    // Do not allow an unknown existing object to hide behind another rejection.
    for (std::uint64_t i = 0; i < input.tile_count(); ++i)
        if (input.object(i) != 0 && !types.find(TypeId{input.object(i)}))
            return Error{ErrorKind::Facts, 0, 0};
    Output out{input, {variation.begin(), variation.end()}, {}, 0};
    for (const auto& record : records) {
        const auto index = static_cast<std::uint64_t>(out.records.size());
        const auto error = [&](ErrorKind kind) { return Error{kind, index, out.consumed}; };
        const auto* type = types.find(record.type);
        // Object id 0 is TileLayer's empty sentinel, not a storable placement.
        if (!type || !record.type.valid() || record.type.v == 0)
            return error(ErrorKind::Facts);
        if ((type->flags_word != 0x09 && type->flags_word != 0x29) || record.rotation > 3 ||
            type->width == 0 || type->height == 0 || record.x < 0 || record.y < 0 ||
            type->width - 1 > std::numeric_limits<std::uint16_t>::max() ||
            type->height - 1 > std::numeric_limits<std::uint16_t>::max())
            return error(ErrorKind::Unsupported);
        const bool odd = (record.rotation & 1U) != 0;
        const auto w = odd ? type->height : type->width;
        const auto h = odd ? type->width : type->height;
        const site::Origin origin{record.x, record.y};
        if (!site::footprint_inside(out.tiles, origin, w, h)) {
            out.records.push_back(Disposition::Bounds);
            continue;
        }
        const auto occupancy = site::footprint_occupancy(out.tiles, types, *type, origin, w, h);
        if (occupancy == site::Compat::Unsupported)
            return error(ErrorKind::Facts);
        if (occupancy == site::Compat::Incompatible) {
            out.records.push_back(Disposition::Occupied);
            continue;
        }
        // AnnoGame 0x10008fa1: mode 1 reads only the origin quad.
        const std::int64_t x = record.x, y = record.y;
        const auto mean =
            static_cast<std::uint8_t>((std::uint32_t{*grid.at(x, y)} + *grid.at(x + 1, y) +
                                       *grid.at(x, y + 1) + *grid.at(x + 1, y + 1)) /
                                      4U);
        // AnnoWelt 0x1007cbdf: group 0x141 bypasses the final ordinary level gate.
        const auto group = ((type->prodtype - 1U) & ~31U) + 1U;
        if (mean < 32 && group != 0x141) {
            out.records.push_back(Disposition::Terrain);
            continue;
        }
        // AnnoWelt 0x10089417: commit before the resource draw and selector.
        place_object_record(out.tiles, types, *type, record.x, record.y, record.rotation,
                            static_cast<std::uint8_t>(record.raw_state & 63U), mean);
        if (type->prodtype >= 225 && type->prodtype <= 256) {
            if (out.consumed == draws.size())
                return error(ErrorKind::Exhausted);
            const auto draw = draws[out.consumed++];
            const auto variant =
                select_prepared(types, prepared.value(), climate, record.type, draw);
            if (!variant)
                return error(variant.error());
            const auto i = out.tiles.index_of(record.x, record.y);
            // Separate island +0x18 plane; do not alias phase or aux.
            out.variation[i] = static_cast<std::uint8_t>((out.variation[i] & 15U) |
                                                         (std::uint32_t{variant.value()} << 4U));
        }
        out.records.push_back(Disposition::Accepted);
    }
    return out;
}
} // namespace anno::sim::resource_import
