#include <algorithm>

#include <anno/app/construction_menu.hpp>

namespace anno::app {

bool ConstructionMenu::inside(double px, double py, int x, int y, unsigned w, unsigned h) {
    return px >= x && px < static_cast<double>(x) + w && py >= y && py < static_cast<double>(y) + h;
}

std::optional<std::string> ConstructionMenu::category_at(double px, double py) const {
    // The column itself (ADR 0004 R1; docs/re/ui.md section 2.1/2.4): x = right chrome + the
    // VIEWBAUTAB gadget's own left edge. The gadget is 42 px wide from x=779 (docs/re/ui.md
    // section 2.4's "(779, click_y-23) 42 x 46"); the column x does not vary by category.
    constexpr double kColumnX = 779.0, kColumnW = 42.0, kHalfHeight = 23.0;
    if (px < kColumnX || px >= kColumnX + kColumnW) {
        return std::nullopt;
    }
    for (const auto& cat : construction_->categories) {
        if (py >= cat.y - kHalfHeight && py < cat.y + kHalfHeight) {
            return cat.name;
        }
    }
    return std::nullopt;
}

std::optional<std::pair<int, int>> ConstructionMenu::cell_at(double px, double py) const {
    const auto& g = construction_->grid;
    if (g.cell_px <= 0 || g.cols <= 0 || g.rows <= 0) {
        return std::nullopt;
    }
    const double dx = px - g.origin_x, dy = py - g.origin_y;
    if (dx < 0 || dy < 0) {
        return std::nullopt;
    }
    const int col = static_cast<int>(dx / g.cell_px);
    const int row = static_cast<int>(dy / g.cell_px);
    if (col < 0 || col >= g.cols || row < 0 || row >= g.rows) {
        return std::nullopt;
    }
    // Inside the stride's gutter (stride 52, sprite 50: a 2 px gap), not on the sprite itself.
    const double within_x = dx - static_cast<double>(col) * g.cell_px;
    const double within_y = dy - static_cast<double>(row) * g.cell_px;
    if (within_x >= g.sprite || within_y >= g.sprite) {
        return std::nullopt;
    }
    return std::make_pair(col, row);
}

ConstructionClickResult
ConstructionMenu::click(double px, double py, const std::string& open_category, bool placement_open,
                        const std::vector<std::pair<sim::TypeId, bool>>* eligibility) const {
    ConstructionClickResult result;

    if (const auto cat = category_at(px, py)) {
        result.kind = ConstructionClickKind::SwitchCategory;
        result.category = *cat;
        return result;
    }
    if (open_category.empty()) {
        return result; // None: the grid, pickaxe and cancel-X are not shown with no category open
    }

    if (placement_open) {
        // Cancel-X (INFOEXIT): docs/re/ui.md section 2.4/2.5 - "on the info/placement panel
        // only; not on the category grid". Checked only in this state: its rectangle
        // ((984,725) 33x38) overlaps the pickaxe's ((970,710) 53x53), and the two panels are
        // never shown together, so this state alone decides which one a click on the overlap
        // means.
        if (const auto* infoexit = hud_->element("infoexit");
            infoexit != nullptr && infoexit->has_pos && infoexit->has_size &&
            inside(px, py, infoexit->pos_x, infoexit->pos_y, infoexit->size_w, infoexit->size_h)) {
            result.kind = ConstructionClickKind::Cancel;
            return result;
        }
    } else {
        // The pickaxe (ABRISSICON): data/ui/hud.txt's own measured rectangle (docs/re/ui.md
        // section 2.4: (970, 710) 53 x 53), not construction.txt's `demolish` click centre (that
        // field is the click point used to drive the Wine runner, not a rectangle). Checked only
        // on the plain category grid (not the placement panel; see above).
        if (const auto* abrissicon = hud_->element("abrissicon");
            abrissicon != nullptr && abrissicon->has_pos && abrissicon->has_size &&
            inside(px, py, abrissicon->pos_x, abrissicon->pos_y, abrissicon->size_w,
                   abrissicon->size_h)) {
            result.kind = ConstructionClickKind::ToggleDemolish;
            return result;
        }
    }

    const auto cell_pos = cell_at(px, py);
    if (!cell_pos) {
        return result; // None: outside the grid, its gutters, or the chrome entirely
    }
    const auto* cell = construction_->cell(open_category, cell_pos->first, cell_pos->second);
    if (cell == nullptr) {
        result.kind = ConstructionClickKind::Empty;
        return result;
    }
    const sim::TypeId type{cell->id};
    bool allowed = false;
    if (eligibility != nullptr) {
        const auto it =
            std::lower_bound(eligibility->begin(), eligibility->end(), type,
                             [](const auto& entry, sim::TypeId t) { return entry.first.v < t.v; });
        if (it != eligibility->end() && it->first == type) {
            allowed = it->second;
        }
    }
    result.category = open_category;
    result.type = type;
    result.kind = allowed ? ConstructionClickKind::Arm : ConstructionClickKind::Locked;
    return result;
}

} // namespace anno::app
