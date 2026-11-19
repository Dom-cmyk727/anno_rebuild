#include <algorithm>

#include <anno/app/construction_menu.hpp>

namespace anno::app {

std::vector<ToolInput> turn_inputs(int turn) {
    const auto kind = turn > 0 ? ToolInput::Kind::Rotate : ToolInput::Kind::RotateBack;
    const std::int64_t steps = turn > 0 ? std::int64_t{turn} : -std::int64_t{turn}; // no -INT_MIN
    return std::vector<ToolInput>(static_cast<std::size_t>(steps), ToolInput{kind, 0, 0});
}

bool ConstructionMenu::inside(double px, double py, int x, int y, unsigned w, unsigned h) {
    return px >= x && px < static_cast<double>(x) + w && py >= y && py < static_cast<double>(y) + h;
}

std::optional<std::string> ConstructionMenu::category_at(double px, double py) const {
    // UI-BUILDCAT-1 (OQ-714): each category's own tab gadget, hud.txt `viewbautab_<k>` (View.dll
    // 0x1002bb4f, one 42 x 46 gadget per category; positions sprite-565 exact on the GOG shots:
    // x 762..803, y 303 + 46k, docs/re/ui.md section 2.4). The gadget rectangle is the hit area,
    // as for every other gadget: the column ends at x 803, so grid column 0 (x 808..857) is the
    // grid's alone. Not `click category_column_x` or construction.txt's `y` (click points for
    // driving the Wine runner, not rectangles). A category without a tab element is not
    // clickable (never a guessed rectangle).
    for (const auto& cat : construction_->categories) {
        const UiElement* tab = construction_category_tab(*hud_, cat);
        if (tab != nullptr && inside(px, py, tab->pos_x, tab->pos_y, tab->size_w, tab->size_h)) {
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

    if (placement_open && !open_category.empty()) {
        // UI-INFO-2: the rotate arrows - every element with a `turn` step (hud.txt rotate_left
        // -1, rotate_right +1: GOG View.dll 0x10030440 / 0x10030480; docs/re/ui.md section 2.5).
        // Checked before the category column (UI-BUILDCAT-1: the column's tabs end at x 803, so
        // rotate_left ((808, 451) 33 x 38) no longer overlaps them; the order is kept).
        for (const UiElement& el : hud_->elements) {
            if (el.turn && el.has_pos && el.has_size &&
                inside(px, py, el.pos_x, el.pos_y, el.size_w, el.size_h)) {
                result.kind = ConstructionClickKind::Turn;
                result.turn = *el.turn;
                return result;
            }
        }
    }
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
        // ((984,725) 33x35) overlaps the pickaxe's ((970,710) 53x53), and the two panels are
        // never shown together, so this state alone decides which one a click on the overlap
        // means.
        if (const auto* infoexit = hud_->element("infoexit");
            infoexit != nullptr && infoexit->has_pos && infoexit->has_size &&
            inside(px, py, infoexit->pos_x, infoexit->pos_y, infoexit->size_w, infoexit->size_h)) {
            result.kind = ConstructionClickKind::Cancel;
            return result;
        }
        // The info/placement panel (data/ui/hud.txt's `info_panel`, (804,302) 220x466) covers the
        // grid's own extent while it is open; nothing else on it (the cost/upkeep chrome) is
        // wired to a command, so any other click here is a no-op, not a fall-through into
        // cell_at() against grid coordinates the panel is drawn over instead of the grid.
        return result; // None: the panel is open and this was neither infoexit nor an arrow
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

ConstructionCellInfo ConstructionMenu::cell_info(sim::TypeId type,
                                                 const sim::TypeRegistry& registry) {
    ConstructionCellInfo info;
    info.type = type;
    const sim::BuildingType* found = registry.find(type);
    if (found == nullptr) {
        return info; // known stays false; the caller shows "unknown type", not a guessed cost
    }
    info.known = true;
    info.cost_gold = found->cost_gold;
    info.cost_goods = found->cost_goods;
    // PROD3's active column (header comment; docs/re/ui.md section 2.5).
    if (found->running_cost.size() == 2) {
        info.upkeep = found->running_cost[1];
    }
    return info;
}

std::optional<InfoPanelContent>
ConstructionMenu::info_panel_content(sim::TypeId type, const sim::TypeRegistry& registry) {
    const ConstructionCellInfo cell = cell_info(type, registry);
    if (!cell.known) {
        return std::nullopt;
    }
    const auto tons = [&](std::string_view goods_name) -> std::int64_t {
        const auto id = registry.goods_by_name(goods_name);
        if (!id) {
            return 0;
        }
        for (const auto& entry : cell.cost_goods) {
            if (entry.goods == *id) {
                return static_cast<std::int64_t>(entry.amount_88 / 256);
            }
        }
        return 0;
    };
    InfoPanelContent out;
    out.type = type;
    out.gold = cell.cost_gold;
    out.tools = tons("WERKZEUG");
    out.wood = tons("HOLZ");
    out.bricks = tons("ZIEGEL");
    out.marble = tons("MAMOR");
    if (cell.upkeep) {
        out.upkeep = *cell.upkeep;
    }
    return out;
}

} // namespace anno::app
