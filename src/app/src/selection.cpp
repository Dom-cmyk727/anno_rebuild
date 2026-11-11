#include <cmath>

#include <anno/app/selection.hpp>

namespace anno::app {

namespace {

std::optional<sim::TileXY> inspect_of(const SelectionState& state) {
    if (state.pending) {
        return state.pending;
    }
    if (state.selected) {
        return state.selected->tile;
    }
    return std::nullopt;
}

SelectionStep done(SelectionState state, bool consumed) {
    SelectionStep step;
    step.inspect = inspect_of(state);
    step.state = state;
    step.consumed = consumed;
    return step;
}

} // namespace

SelectionStep selection_step(const SelectionState& state, const SelectionEvent& event) {
    const bool active = state.pending.has_value() || state.selected.has_value();
    switch (event.kind) {
    case SelectionEventKind::Click:
        if (event.mode != PointerMode::Select) {
            // The tool's click (place, demolish, developer tool): the selection stays as it is.
            return done(state, false);
        }
        if (!event.tile) {
            // Off the map: deselect (QUESTION K-3, recommended), the click is ours only if there
            // was something to deselect.
            return done(SelectionState{}, active);
        }
        {
            SelectionState next = state;
            next.pending = event.tile;
            return done(next, true);
        }
    case SelectionEventKind::Escape:
        // With a selection: deselect. Without one: not ours (today's Escape behaviour).
        return done(active ? SelectionState{} : state, active);
    case SelectionEventKind::Answer: {
        SelectionState next = state;
        if (!event.tile) {
            return done(next, false);
        }
        if (state.pending && *event.tile == *state.pending) {
            next.pending.reset();
            if (event.building) {
                next.selected = SelectedBuilding{*event.building, *event.tile};
            } else {
                next.selected.reset(); // empty ground, or not the player's building: deselect
            }
            return done(next, false);
        }
        if (!state.pending && state.selected && *event.tile == state.selected->tile &&
            (!event.building || *event.building != state.selected->id)) {
            next.selected.reset(); // demolished, or replaced by another building
        }
        return done(next, false);
    }
    }
    return done(state, false);
}

bool is_click(double dx, double dy, double slop) {
    return std::fabs(dx) <= slop && std::fabs(dy) <= slop;
}

} // namespace anno::app
