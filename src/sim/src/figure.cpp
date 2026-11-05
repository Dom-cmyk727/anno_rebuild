// The figure registry (docs/sim/state.md §4.9, §5.9; stages S7 and S12, task T22).
#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <anno/sim/figure.hpp>
#include <anno/sim/tick.hpp>

namespace anno::sim {

FigureRegistry make_figure_registry(const SimConfig& cfg) {
    FigureRegistry r;
    r.list = SlotList<Figure>(cfg.list_initial_size);
    return r;
}

namespace {

bool sails(const Figure& f) {
    return f.nav.has_value() && !f.parent.valid();
}

std::pair<std::int32_t, std::int32_t> cell_key(FixedPos p) {
    return {figure_cell_of(p.y), figure_cell_of(p.x)};
}

void index_figure(FigureRegistry& r, const Figure& f) {
    if (f.parent.valid()) {
        auto& kids = r.children[f.parent];
        kids.insert(std::lower_bound(kids.begin(), kids.end(), f.id), f.id);
    }
    if (f.nav.has_value() && f.nav->ticket.has_value()) {
        r.tickets.insert(*f.nav->ticket);
    }
    if (sails(f)) {
        r.ship_cells[cell_key(f.pos)].insert(f.id);
    }
}

void unindex_figure(FigureRegistry& r, const Figure& f) {
    if (f.parent.valid()) {
        const auto it = r.children.find(f.parent);
        if (it != r.children.end()) {
            auto& kids = it->second;
            const auto k = std::lower_bound(kids.begin(), kids.end(), f.id);
            if (k != kids.end() && *k == f.id) {
                kids.erase(k);
            }
            if (kids.empty()) {
                r.children.erase(it);
            }
        }
    }
    if (f.nav.has_value() && f.nav->ticket.has_value()) {
        r.tickets.erase(*f.nav->ticket);
    }
    if (sails(f)) {
        const auto it = r.ship_cells.find(cell_key(f.pos));
        if (it != r.ship_cells.end()) {
            it->second.erase(f.id);
            if (it->second.empty()) {
                r.ship_cells.erase(it);
            }
        }
    }
}

} // namespace

std::int32_t figure_cell_of(std::int32_t q8) {
    return q8 < 0 ? 0 : (q8 / 256) >> 4;
}

void note_ticket(FigureRegistry& registry, std::optional<std::uint64_t> before,
                 std::optional<std::uint64_t> after) {
    if (before == after) {
        return;
    }
    if (before.has_value()) {
        registry.tickets.erase(*before);
    }
    if (after.has_value()) {
        registry.tickets.insert(*after);
    }
}

void note_ship_moved(FigureRegistry& registry, const Figure& ship, FixedPos before) {
    if (!sails(ship)) {
        return;
    }
    const auto from = cell_key(before);
    const auto to = cell_key(ship.pos);
    if (from == to) {
        return;
    }
    const auto it = registry.ship_cells.find(from);
    if (it != registry.ship_cells.end()) {
        it->second.erase(ship.id);
        if (it->second.empty()) {
            registry.ship_cells.erase(it);
        }
    }
    registry.ship_cells[to].insert(ship.id);
}

std::string figure_index_problem(const FigureRegistry& registry) {
    FigureRegistry fresh;
    fresh.list = registry.list;
    rebuild_figure_index(fresh);
    if (fresh.by_id != registry.by_id) {
        return "figure index: by_id differs from a fresh reconstruction";
    }
    if (fresh.children != registry.children) {
        return "figure index: children differs from a fresh reconstruction";
    }
    if (fresh.tickets != registry.tickets) {
        return "figure index: tickets differs from a fresh reconstruction";
    }
    if (fresh.ship_cells != registry.ship_cells) {
        return "figure index: ship_cells differs from a fresh reconstruction (a ship was moved or "
               "re-parented without note_ship_moved)";
    }
    return {};
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
    index_figure(registry, *registry.list.slots[slot]);
    return slot;
}

Slot add_figure_at(FigureRegistry& registry, Figure f, Slot slot, const SimConfig& cfg) {
    const FigureId id = f.id;
    if (!id.valid() || id.v == 0 || registry.by_id.contains(id) || slot >= kListSizeBound ||
        (slot < registry.list.slots.size() && registry.list.slots[slot].has_value())) {
        return kNoSlot;
    }
    if (slot >= registry.list.slots.size()) {
        // Explicit slot growth is based on the requested slot (Welt 0x1009734b),
        // unlike a normal free-slot insert. grown_size also retains the small-config floor.
        const auto grown = SlotList<Figure>::grown_size(slot, cfg.figure_min_growth);
        if (grown == 0 || grown > kListSizeBound) {
            return kNoSlot;
        }
        registry.list.slots.resize(static_cast<std::size_t>(grown));
    }
    registry.list.slots[slot] = std::move(f);
    registry.list.hint = slot;
    registry.by_id[id] = slot;
    index_figure(registry, *registry.list.slots[slot]);
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
    registry.children.clear();
    registry.tickets.clear();
    registry.ship_cells.clear();
    for (std::size_t s = 0; s < registry.list.slots.size(); ++s) {
        if (registry.list.slots[s].has_value()) {
            registry.by_id[registry.list.slots[s]->id] = static_cast<Slot>(s);
            index_figure(registry, *registry.list.slots[s]);
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

/// MAP-ECON-1-CLOSE-1: the harvest job's terminal state on the shared end path (docs/sim/
/// forestry.md 2.1, 3.5). Runs before release_home clears the home id, which names the island of
/// the target. Done (the body's own end, already normalized) is left untouched, so the ordinary
/// completion's bytes do not change; repeated calls do nothing.
void finish_harvest(Figure& figure, Effects& fx) {
    if (!figure.harvest.has_value() || figure.harvest->step == HarvestStep::Done) {
        return;
    }
    HarvestState& h = *figure.harvest;
    if (h.step == HarvestStep::ToTree || h.step == HarvestStep::Cut) {
        // Only these steps hold a claim (Start has not selected, the cut arrival released it before
        // ToHome). Owner-conditional and hygiene-gated at the merge: never another worker's claim.
        fx.tile_edits.push_back(
            TileEdit{TileEdit::Kind::Release, island_of(figure.home), h.target, figure.id});
    }
    fx.harvest_lost_88 += figure.cargo_88; // the cargo stays on the figure as history
    h.step = HarvestStep::Done;
    h.start_delay = 0;
    h.wait = 0;
    h.route.clear();
    h.next = 0;
    h.along = 0;
}

/// MAP-POP-LIVE-1 L2: the resident job's terminal state on the shared end path (docs/sim/
/// population-live.md section 3). Done (the body's own end, already normalized) is left untouched;
/// the cargo stays on the figure as history. Repeated calls do nothing.
void finish_resident(Figure& figure) {
    if (!figure.resident.has_value() || figure.resident->step == ResidentStep::Done) {
        return;
    }
    ResidentState& r = *figure.resident;
    r.step = ResidentStep::Done;
    r.wait = 0;
    r.target = BuildingId{};
    r.route.clear();
    r.next = 0;
    r.along = 0;
}

/// D 0x1003b6e0's release: the home counter through the figure, once (the unit's +0x78
/// is cleared after the home's slot +0x2c).
void release_home(Figure& figure, Effects& fx) {
    if (!figure.home.valid()) {
        return;
    }
    ReleaseCounter r;
    r.home = figure.home;
    r.home_counter = figure.home_counter;
    r.holder = figure.id;
    fx.releases.push_back(r);
    figure.home = BuildingId{};
}

} // namespace

std::optional<Figure> remove_figure(FigureRegistry& registry, FigureId id) {
    erase_id(registry.expiry, id);
    const Slot s = slot_of(registry, id);
    if (s == kNoSlot) {
        return std::nullopt;
    }
    std::optional<Figure> out;
    out.swap(registry.list.slots[s]);
    unindex_figure(registry, *out);
    registry.list.remove(s);
    registry.by_id.erase(id);
    return out;
}

bool end_figure(Figure& figure, std::uint32_t t, Effects& fx) {
    if (figure.expiry_stamp.has_value()) {
        return false;
    }
    finish_harvest(figure, fx);
    finish_resident(figure);
    release_home(figure, fx);
    figure.expiry_stamp = static_cast<std::uint16_t>(t);
    return true;
}

void end_figure(FigureRegistry& registry, Figure& figure, std::uint32_t t, Effects& fx) {
    if (end_figure(figure, t, fx)) {
        registry.expiry.push_back(figure.id);
    }
}

void sync_figure_lists(FigureRegistry& registry, Figure& figure, bool had_stamp, Effects& fx) {
    const bool has_stamp = figure.expiry_stamp.has_value();
    if (has_stamp) {
        // A stamp means the figure has ended: its counter goes back now if the seam did
        // not end it through SerialWorld::end_figure (own design, the seam protocol); an active
        // harvest job is finished first, as end_figure does.
        finish_harvest(figure, fx);
        finish_resident(figure);
        release_home(figure, fx);
    }
    if (has_stamp && !had_stamp) {
        registry.expiry.push_back(figure.id);
    } else if (!has_stamp && had_stamp) {
        erase_id(registry.expiry, figure.id);
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
    // The expiry list: members of the registry, each once, and a figure's stamp says
    // whether it is on it (phase C reads the stamp of the list's front).
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
    for (std::size_t s = 0; s < l.slots.size(); ++s) {
        if (!l.slots[s].has_value()) {
            continue;
        }
        const Figure& f = *l.slots[s];
        const std::string where = "figures.list.slots[" + std::to_string(s) + "]";
        if (f.expiry_stamp.has_value() != on_expiry.contains(f.id)) {
            return where + ".expiry_stamp: the figure " +
                   (f.expiry_stamp.has_value() ? "has a stamp but is not" : "has no stamp but is") +
                   " on the expiry list";
        }
        // The end path releases before it stamps (0x1003b6e0, then the Remove at
        // 0x10034844): an ended figure holds no counter.
        if (f.expiry_stamp.has_value() && f.home.valid()) {
            return where + ".home: the figure has ended (it has an expiry_stamp) but still " +
                   "holds a counter of building " + std::to_string(f.home.v);
        }
        // The transport state (T24): a step the job knows, and a walk position inside
        // the route (the walk code indexes route[next - 1] and route[next]).
        const TransportState& tr = f.transport;
        if (static_cast<std::uint8_t>(tr.step) > static_cast<std::uint8_t>(TransportStep::Done)) {
            return where + ".transport.step: " + std::to_string(static_cast<unsigned>(tr.step)) +
                   " is no step";
        }
        if (!tr.route.empty() && (tr.next == 0 || tr.next > tr.route.size())) {
            return where + ".transport.next: " + std::to_string(tr.next) + " is not in 1 .. " +
                   std::to_string(tr.route.size());
        }
        if (tr.route.empty() && (tr.next != 0 || tr.along != 0)) {
            return where + ".transport: no route, but next " + std::to_string(tr.next) +
                   " and along " + std::to_string(tr.along);
        }
        // MAP-ECON-1 E2: the harvest job's own facts (docs/sim/forestry.md 2, 2.1). The facts that
        // need the buildings or the tiles (the home's payload, the target's island, the claims) are
        // World::check_invariants'. Nothing here requires an existing home: a demolished home is a
        // valid state until the worker's next arrival.
        if (f.harvest.has_value()) {
            const HarvestState& h = *f.harvest;
            const std::string hw = where + ".harvest";
            if (!harvest_state_job(f.job)) {
                return hw + ": present on a figure of job " + std::to_string(f.job) +
                       " (only the harvest jobs 0x29 and 0x2a and the animal job 0x2e have one)";
            }
            if (static_cast<std::uint8_t>(h.step) > static_cast<std::uint8_t>(HarvestStep::Done)) {
                return hw + ".step: " + std::to_string(static_cast<unsigned>(h.step)) +
                       " is no step";
            }
            if (h.start_delay > kHarvestStartDelay) {
                return hw + ".start_delay: " + std::to_string(h.start_delay) + " is above " +
                       std::to_string(kHarvestStartDelay);
            }
            if (h.start_delay > 0 &&
                (h.step != HarvestStep::Start || !h.route.empty() || f.cargo_88 != 0)) {
                return hw + ".start_delay: " + std::to_string(h.start_delay) +
                       " ticks still owed, but the job has started (a step, a route or cargo)";
            }
            if ((h.step == HarvestStep::Cut) != (h.wait > 0)) {
                return hw + ".wait: " + std::to_string(h.wait) + " in step " +
                       std::to_string(static_cast<unsigned>(h.step)) +
                       " (a wait is owed exactly in Cut, and a finished wait is resolved at once)";
            }
            if (!h.route.empty() && (h.next == 0 || h.next > h.route.size())) {
                return hw + ".next: " + std::to_string(h.next) + " is not in 1 .. " +
                       std::to_string(h.route.size());
            }
            if (h.route.empty() && (h.next != 0 || h.along != 0)) {
                return hw + ": no route, but next " + std::to_string(h.next) + " and along " +
                       std::to_string(h.along);
            }
            if (!h.route.empty() && h.step != HarvestStep::ToTree &&
                h.step != HarvestStep::ToHome) {
                return hw + ".route: a walk outside the walking steps ToTree and ToHome";
            }
            if (h.step == HarvestStep::Done && !f.expiry_stamp.has_value()) {
                return hw + ".step: Done, but the figure has not ended (Done is resolved in the "
                            "same call by its end)";
            }
            // MAP-ECON-1-CLOSE-1: and every end path finishes the job first (Done <=> ended).
            if (f.expiry_stamp.has_value() && h.step != HarvestStep::Done) {
                return hw + ".step: " + std::to_string(static_cast<unsigned>(h.step)) +
                       ", but the figure has ended (every end path finishes the job: Done)";
            }
        }
    }
    // MAP-POP-LIVE-1 L2: the resident job's own facts (docs/sim/population-live.md section 3); the
    // facts that need the buildings (the home's payload and walker slot) are
    // World::check_invariants'.
    for (std::size_t s = 0; s < l.slots.size(); ++s) {
        if (!l.slots[s].has_value() || !l.slots[s]->resident.has_value()) {
            continue;
        }
        const Figure& f = *l.slots[s];
        const ResidentState& r = *f.resident;
        const std::string rw = "figures.list.slots[" + std::to_string(s) + "].resident";
        if (f.job != kJobResidentCode) {
            return rw + ": present on a figure of job " + std::to_string(f.job) +
                   " (only the house walker job 0x35 has one)";
        }
        if (static_cast<std::uint8_t>(r.step) > static_cast<std::uint8_t>(ResidentStep::Done)) {
            return rw + ".step: " + std::to_string(static_cast<unsigned>(r.step)) + " is no step";
        }
        if ((r.step == ResidentStep::Wait) != (r.wait > 0)) {
            return rw + ".wait: " + std::to_string(r.wait) + " in step " +
                   std::to_string(static_cast<unsigned>(r.step)) +
                   " (a wait is owed exactly in Wait, and a finished wait is resolved at once)";
        }
        if (!r.route.empty() && (r.next == 0 || r.next > r.route.size())) {
            return rw + ".next: " + std::to_string(r.next) + " is not in 1 .. " +
                   std::to_string(r.route.size());
        }
        if (r.route.empty() && (r.next != 0 || r.along != 0)) {
            return rw + ": no route, but next " + std::to_string(r.next) + " and along " +
                   std::to_string(r.along);
        }
        if (!r.route.empty() && r.step != ResidentStep::ToStall && r.step != ResidentStep::ToHome) {
            return rw + ".route: a walk outside the walking steps ToStall and ToHome";
        }
        if ((r.step == ResidentStep::Done) != f.expiry_stamp.has_value()) {
            return rw + ".step: " + std::to_string(static_cast<unsigned>(r.step)) +
                   (f.expiry_stamp.has_value()
                        ? ", but the figure has ended (every end path finishes the job: Done)"
                        : ", Done but the figure has not ended (Done is resolved in the same call "
                          "by its end)");
        }
    }
    // MAP-2: the facing, the parent links and the crew slots. Every figure of the registry
    // is in `seen` (ordered), so a parent is looked up, never searched for. The chain check
    // colours each figure once (0 unvisited, 1 on the walk now, 2 known to end), so the whole
    // pass is O(n log n) however deep the nesting is.
    std::map<FigureId, const Figure*> by_figure;
    for (std::size_t s = 0; s < l.slots.size(); ++s) {
        if (l.slots[s].has_value()) {
            by_figure.emplace(l.slots[s]->id, &*l.slots[s]);
        }
    }
    std::set<std::pair<FigureId, std::uint32_t>> crew_taken;
    for (std::size_t s = 0; s < l.slots.size(); ++s) {
        if (!l.slots[s].has_value()) {
            continue;
        }
        const Figure& f = *l.slots[s];
        const std::string where = "figures.list.slots[" + std::to_string(s) + "]";
        if (f.dir16 > 15) {
            return where + ".dir16: " + std::to_string(f.dir16) + " is not in 0 .. 15";
        }
        if (f.parent.valid()) {
            if (f.parent == f.id) {
                return where + ".parent: figure " + std::to_string(f.id.v) + " is its own parent";
            }
            const auto up = by_figure.find(f.parent);
            if (up == by_figure.end()) {
                return where + ".parent: figure " + std::to_string(f.parent.v) +
                       " is not in the registry";
            }
            if (up->second->owner != f.owner) {
                return where + ".owner: " + std::to_string(f.owner.v) + " differs from the owner " +
                       std::to_string(up->second->owner.v) + " of its parent " +
                       std::to_string(f.parent.v);
            }
        }
        if (f.crew_slot.has_value()) {
            if (!f.parent.valid()) {
                return where + ".crew_slot: " + std::to_string(*f.crew_slot) +
                       ", but the figure has no parent";
            }
            if (!crew_taken.emplace(f.parent, *f.crew_slot).second) {
                return where + ".crew_slot: slot " + std::to_string(*f.crew_slot) + " of figure " +
                       std::to_string(f.parent.v) + " is used twice";
            }
        }
    }
    {
        std::map<FigureId, std::uint8_t> colour;
        std::vector<FigureId> walk;
        for (std::size_t s = 0; s < l.slots.size(); ++s) {
            if (!l.slots[s].has_value()) {
                continue;
            }
            walk.clear();
            FigureId at = l.slots[s]->id;
            while (at.valid()) {
                std::uint8_t& c = colour[at];
                if (c == 2) {
                    break;
                }
                if (c == 1) {
                    return "figures.list.slots[" + std::to_string(s) +
                           "].parent: the parent links of figure " + std::to_string(at.v) +
                           " form a cycle";
                }
                c = 1;
                walk.push_back(at);
                at = by_figure.at(at)->parent;
            }
            for (const FigureId done : walk) {
                colour[done] = 2;
            }
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
