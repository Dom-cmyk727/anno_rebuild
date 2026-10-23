// The figure registry (docs/sim/state.md §4.9, §5.9; stages S7 and S12).
#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <anno/sim/figure.hpp>

namespace anno::sim {

FigureRegistry make_figure_registry(const SimConfig& cfg) {
    FigureRegistry r;
    r.list = SlotList<Figure>(cfg.list_initial_size);
    return r;
}

Slot add_figure(FigureRegistry& registry, Figure f, const SimConfig& cfg, const Rules& rules) {
    const FigureId id = f.id;
    if (!id.valid() || registry.by_id.contains(id)) {
        return kNoSlot;
    }
    const Slot slot =
        registry.list.insert(std::move(f), SlotParams{cfg.figure_min_growth, rules.slot_policy});
    if (slot == kNoSlot) {
        return kNoSlot;
    }
    registry.by_id[id] = slot;
    return slot;
}

namespace {

/// The slot of the figure `id` when the id map points at it, else kNoSlot.
Slot slot_of(const FigureRegistry& registry, FigureId id) {
    const auto it = registry.by_id.find(id);
    if (it == registry.by_id.end()) {
        return kNoSlot;
    }
    const Slot s = it->second;
    if (s >= registry.list.slots.size() || !registry.list.slots[s].has_value() ||
        registry.list.slots[s]->id != id) {
        return kNoSlot;
    }
    return s;
}

} // namespace

const Figure* find_figure(const FigureRegistry& registry, FigureId id) {
    const Slot s = slot_of(registry, id);
    return s == kNoSlot ? nullptr : &*registry.list.slots[s];
}

Figure* find_figure(FigureRegistry& registry, FigureId id) {
    const Slot s = slot_of(registry, id);
    return s == kNoSlot ? nullptr : &*registry.list.slots[s];
}

void rebuild_figure_index(FigureRegistry& registry) {
    registry.by_id.clear();
    for (std::size_t s = 0; s < registry.list.slots.size(); ++s) {
        if (registry.list.slots[s].has_value()) {
            registry.by_id[registry.list.slots[s]->id] = static_cast<Slot>(s);
        }
    }
}

namespace {

void erase_id(std::vector<FigureId>& list, FigureId id) {
    const auto it = std::find(list.begin(), list.end(), id);
    if (it != list.end()) {
        list.erase(it);
    }
}

} // namespace

std::optional<Figure> remove_figure(FigureRegistry& registry, FigureId id) {
    erase_id(registry.expiry, id);
    erase_id(registry.deferred, id);
    const Slot s = slot_of(registry, id);
    if (s == kNoSlot) {
        return std::nullopt;
    }
    std::optional<Figure> out = std::move(registry.list.slots[s]);
    registry.list.remove(s);
    registry.by_id.erase(id);
    return out;
}

void mark_figure_for_delete(FigureRegistry& registry, Figure& figure) {
    if (figure.marked_for_delete) {
        return;
    }
    figure.marked_for_delete = true;
    registry.deferred.push_back(figure.id);
}

void sync_figure_lists(FigureRegistry& registry, const Figure& figure, bool had_stamp,
                       bool was_marked) {
    const bool has_stamp = figure.expiry_stamp.has_value();
    if (has_stamp && !had_stamp) {
        registry.expiry.push_back(figure.id);
    } else if (!has_stamp && had_stamp) {
        erase_id(registry.expiry, figure.id);
    }
    if (figure.marked_for_delete && !was_marked) {
        registry.deferred.push_back(figure.id);
    } else if (!figure.marked_for_delete && was_marked) {
        erase_id(registry.deferred, figure.id);
    }
}

std::string validate_figures(const FigureRegistry& registry, const IdCounters& ids) {
    const SlotList<Figure>& l = registry.list;
    const std::uint64_t n = l.slots.size();
    if (n < 1) {
        return "figures.list: has no slot (a list has at least one element)";
    }
    if (l.hint < -1 || l.hint >= static_cast<std::int64_t>(n)) {
        return "figures.list.hint: " + std::to_string(l.hint) + " is not in -1 .. " +
               std::to_string(n - 1);
    }
    if (l.cursor >= n) {
        return "figures.list.cursor: " + std::to_string(l.cursor) + " is not below the " +
               std::to_string(n) + " slots";
    }
    std::set<FigureId> seen;
    for (std::size_t s = 0; s < l.slots.size(); ++s) {
        if (!l.slots[s].has_value()) {
            continue;
        }
        const Figure& f = *l.slots[s];
        const std::string where = "figures.list.slots[" + std::to_string(s) + "]";
        if (f.id.v == 0 || f.id.v >= ids.next_figure) {
            return where + ".id: " + std::to_string(f.id.v) + " is not in 1 .. next_figure (" +
                   std::to_string(ids.next_figure) + ")";
        }
        if (!seen.insert(f.id).second) {
            return where + ".id: " + std::to_string(f.id.v) + " is used twice";
        }
    }
    // The expiry and deferred lists: members of the registry, each once, and the
    // flags of a figure say which list it is on (S12's destroy and release read
    // both, and a flag that disagrees with its list would be released twice).
    std::set<FigureId> on_expiry;
    for (std::size_t i = 0; i < registry.expiry.size(); ++i) {
        const FigureId id = registry.expiry[i];
        if (!seen.contains(id)) {
            return "figures.expiry[" + std::to_string(i) + "]: figure " + std::to_string(id.v) +
                   " is not in the registry";
        }
        if (!on_expiry.insert(id).second) {
            return "figures.expiry[" + std::to_string(i) + "]: figure " + std::to_string(id.v) +
                   " is on the list twice";
        }
    }
    std::set<FigureId> on_deferred;
    for (std::size_t i = 0; i < registry.deferred.size(); ++i) {
        const FigureId id = registry.deferred[i];
        if (!seen.contains(id)) {
            return "figures.deferred[" + std::to_string(i) + "]: figure " + std::to_string(id.v) +
                   " is not in the registry";
        }
        if (!on_deferred.insert(id).second) {
            return "figures.deferred[" + std::to_string(i) + "]: figure " + std::to_string(id.v) +
                   " is on the list twice";
        }
    }
    for (std::size_t s = 0; s < l.slots.size(); ++s) {
        if (!l.slots[s].has_value()) {
            continue;
        }
        const Figure& f = *l.slots[s];
        const std::string where = "figures.list.slots[" + std::to_string(s) + "]";
        if (f.marked_for_delete != on_deferred.contains(f.id)) {
            return where + ".marked_for_delete: " + (f.marked_for_delete ? "true" : "false") +
                   " but the figure is " + (f.marked_for_delete ? "not " : "") +
                   "on the deferred list";
        }
        if (f.expiry_stamp.has_value() != on_expiry.contains(f.id)) {
            return where + ".expiry_stamp: the figure " +
                   (f.expiry_stamp.has_value() ? "has a stamp but is not"
                                                : "has no stamp but is") +
                   " on the expiry list";
        }
    }
    std::map<FigureId, Slot> fresh;
    for (std::size_t s = 0; s < l.slots.size(); ++s) {
        if (l.slots[s].has_value()) {
            fresh[l.slots[s]->id] = static_cast<Slot>(s);
        }
    }
    if (fresh != registry.by_id) {
        return "figures.by_id: the id map differs from a recomputation";
    }
    return {};
}

} // namespace anno::sim
