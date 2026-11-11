// The selected building of the classic surface (ADR 0004 "Selection", W4-NEXT-2 part K, screen
// row 18). Viewer UI state only: no sim state, no command. A pure decision over (state, event)
// so every rule is unit-tested without a window (tests/selection_test.cpp).
//
// A click only asks: the viewer requests an inspection of the clicked tile
// (PlayRunner::request_inspect) and the next published snapshot answers which building of the
// player's city stands there (PlaySnapshot::inspection). The answer selects it, or deselects on
// empty ground. Every later answer for the selected tile re-checks the building's id, so a
// building demolished (or replaced) meanwhile drops the selection. Pan and zoom do not touch the
// state: it holds a building id and a map tile, never a screen position.
#pragma once

#include <optional>

#include <anno/sim/ids.hpp>

namespace anno::app {

/// What a left click means right now (ADR 0004 "Selection"): only `Select` lets this module
/// have it; the others are the construction / demolish / developer tool's, unchanged.
enum class PointerMode { Select, Build, Demolish, DevTool };

struct SelectedBuilding {
    sim::BuildingId id;
    /// The tile that was clicked and is inspected (any tile of the building's footprint).
    sim::TileXY tile;
    friend bool operator==(const SelectedBuilding&, const SelectedBuilding&) = default;
};

struct SelectionState {
    /// A click asked about this tile; the next snapshot's answer for it resolves the click.
    std::optional<sim::TileXY> pending;
    /// The selected building (kept while a new click is pending, so the panel does not flicker).
    std::optional<SelectedBuilding> selected;
    friend bool operator==(const SelectionState&, const SelectionState&) = default;
};

enum class SelectionEventKind {
    /// A left click (press and release without a drag) in the world, not on a UI rectangle.
    Click,
    /// The Escape key, after the construction menu (which takes it while open) passed it on.
    Escape,
    /// A published snapshot's inspection answer.
    Answer,
};

struct SelectionEvent {
    SelectionEventKind kind = SelectionEventKind::Click;
    /// Click: the mode the pointer is in.
    PointerMode mode = PointerMode::Select;
    /// Click: the map tile under the pointer (nullopt: off the map).
    /// Answer: the tile the inspection answers (nullopt: the snapshot answers no tile).
    std::optional<sim::TileXY> tile;
    /// Answer: the player's building on that tile (nullopt: none).
    std::optional<sim::BuildingId> building;
};

struct SelectionStep {
    SelectionState state;
    /// The event was the selection's: the window does nothing else with it (a consumed Escape
    /// does not quit; a consumed click does not pan).
    bool consumed = false;
    /// The tile to keep inspecting (request_inspect) after this step: the pending tile, else the
    /// selected building's, else none.
    std::optional<sim::TileXY> inspect;
};

[[nodiscard]] SelectionStep selection_step(const SelectionState& state,
                                           const SelectionEvent& event);

/// Whether a press and its release `dx`, `dy` physical pixels apart are a click (select) rather
/// than a drag (pan): both within `slop` pixels (ADR 0004 "Selection", QUESTION K-2).
[[nodiscard]] bool is_click(double dx, double dy, double slop);

} // namespace anno::app
