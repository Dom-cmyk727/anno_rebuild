// Building payloads and the pure helpers around them (docs/sim/state.md §4.6, §7.3,
// §9.6, stage S6).
#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <anno/sim/building.hpp>

namespace anno::sim {

BuildingIndex make_building_index(const SimConfig& cfg) {
    BuildingIndex index;
    index.lists.reserve(cfg.layout.lists.size());
    for (std::size_t i = 0; i < cfg.layout.lists.size(); ++i) {
        index.lists.emplace_back(cfg.list_initial_size);
    }
    index.service_counters.assign(cfg.service_bits, 0);
    index.house_counts.assign(cfg.house_groups, 0);
    return index;
}

std::pair<std::uint32_t, std::uint32_t> footprint(const BuildingType& type, std::uint8_t rotation) {
    if ((rotation & 1U) != 0) {
        return {type.height, type.width};
    }
    return {type.width, type.height};
}

std::optional<std::uint32_t> house_group(const SimConfig& cfg, std::uint32_t prodtype) {
    const ListLayout& layout = cfg.layout;
    if (layout.block_size == 0 || prodtype < layout.first_prodtype) {
        return std::nullopt;
    }
    // The blocks that map to the house list, in block order: the group numbers run on
    // across them (the k-th such block holds groups k * block_size ..), so a layout
    // with two house blocks counts the houses of both.
    std::uint64_t rank = 0;
    for (std::size_t b = 0; b < layout.block_to_list.size(); ++b) {
        if (layout.block_to_list[b] != layout.roles.house_list) {
            continue;
        }
        const std::uint64_t start =
            std::uint64_t{layout.first_prodtype} + std::uint64_t{layout.block_size} * b;
        if (prodtype >= start && prodtype - start < layout.block_size) {
            const std::uint64_t g = rank * layout.block_size + (prodtype - start);
            if (g >= cfg.house_groups) {
                return std::nullopt;
            }
            return static_cast<std::uint32_t>(g);
        }
        ++rank;
    }
    return std::nullopt;
}

Payload make_payload(const BuildingType& type, const ClassInfo& cls, const SimConfig& cfg) {
    switch (cls.payload) {
    case PayloadTag::Workshop:
        return make_workshop(type.prod);
    case PayloadTag::WeaponsWorkshop:
        return make_weapons_workshop(type.prod);
    case PayloadTag::Farm:
        return Farm{};
    case PayloadTag::SmallFarm:
        return make_small_farm(type.prod, cfg.small_farm_animals);
    case PayloadTag::Mine:
        return Mine{};
    case PayloadTag::Market:
        return Market{};
    case PayloadTag::Inert:
        break;
    }
    return Inert{};
}

namespace {

const char* payload_name(std::size_t index) {
    switch (index) {
    case 0:
        return "Inert";
    case 1:
        return "Workshop";
    case 2:
        return "WeaponsWorkshop";
    case 3:
        return "Farm";
    case 4:
        return "SmallFarm";
    case 5:
        return "Mine";
    case 6:
        return "Market";
    default:
        return "?";
    }
}

std::string sizes(const char* what, std::size_t have, std::size_t want) {
    return std::string(what) + " has " + std::to_string(have) + " entries, the type needs " +
           std::to_string(want);
}

} // namespace

std::string payload_problem(const Payload& payload, const BuildingType& type, const ClassInfo& cls,
                            const SimConfig& cfg) {
    const std::size_t want = static_cast<std::size_t>(cls.payload);
    if (payload.index() != want) {
        return std::string("holds ") + payload_name(payload.index()) + ", the class needs " +
               payload_name(want);
    }
    // The reference payload as make_payload sizes it: the step functions index
    // out / in / activity / line_on / figures_out by these counts.
    const Payload ref = make_payload(type, cls, cfg);
    if (const auto* w = std::get_if<Workshop>(&payload)) {
        const auto& r = std::get<Workshop>(ref);
        if (w->out.size() != r.out.size()) {
            return sizes("out", w->out.size(), r.out.size());
        }
        if (w->in.size() != r.in.size()) {
            return sizes("in", w->in.size(), r.in.size());
        }
        if (w->activity.size() != r.activity.size()) {
            return sizes("activity", w->activity.size(), r.activity.size());
        }
        if (w->reserved.size() != r.reserved.size()) {
            return sizes("reserved", w->reserved.size(), r.reserved.size());
        }
    } else if (const auto* ww = std::get_if<WeaponsWorkshop>(&payload)) {
        const auto& r = std::get<WeaponsWorkshop>(ref);
        if (ww->line_on.size() != r.line_on.size()) {
            return sizes("line_on", ww->line_on.size(), r.line_on.size());
        }
        if (ww->out.size() != r.out.size()) {
            return sizes("out", ww->out.size(), r.out.size());
        }
        if (ww->in.size() != r.in.size()) {
            return sizes("in", ww->in.size(), r.in.size());
        }
        if (ww->activity.size() != r.activity.size()) {
            return sizes("activity", ww->activity.size(), r.activity.size());
        }
        if (ww->reserved.size() != r.reserved.size()) {
            return sizes("reserved", ww->reserved.size(), r.reserved.size());
        }
    } else if (const auto* sf = std::get_if<SmallFarm>(&payload)) {
        const auto& r = std::get<SmallFarm>(ref);
        if (sf->line_on.size() != r.line_on.size()) {
            return sizes("line_on", sf->line_on.size(), r.line_on.size());
        }
        if (sf->out.size() != r.out.size()) {
            return sizes("out", sf->out.size(), r.out.size());
        }
        if (sf->raw.size() != r.raw.size()) {
            return sizes("raw", sf->raw.size(), r.raw.size());
        }
        if (sf->activity.size() != r.activity.size()) {
            return sizes("activity", sf->activity.size(), r.activity.size());
        }
        if (sf->figures_out.size() != r.figures_out.size()) {
            return sizes("figures_out", sf->figures_out.size(), r.figures_out.size());
        }
        if (sf->reserved.size() != r.reserved.size()) {
            return sizes("reserved", sf->reserved.size(), r.reserved.size());
        }
    } else if (std::holds_alternative<Mine>(payload)) {
        if (type.prod.outputs.empty()) {
            return "a Mine type has no output goods";
        }
    }
    return {};
}

MineArea mine_area(const Building& b, const BuildingType& type) {
    const auto [w, h] = footprint(type, b.rotation);
    return MineArea{b.x, b.y, w, h};
}

} // namespace anno::sim
