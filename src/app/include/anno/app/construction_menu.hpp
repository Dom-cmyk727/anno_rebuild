// UI.2 C1/C2/C4/C5: the construction menu's click resolution. Turns a click in HUD-reference
// drawable pixels (1024 x 768, docs/re/ui.md section 2) into what to do, reading data/ui/'s own
// rectangles (ADR 0004 R1's "no coordinate lives in code") and the sim's own published
// eligibility (PlaySnapshot::eligibility, C2's lock authority) - never
// data/ui/construction.txt's own `state` column, which is RE evidence about the original, not a
// live rule. This class only decides; it sends nothing itself - the caller acts on the result
// through DevTool (arm(), or the ordinary ToggleDemolish/Cancel events), the same command path
// the developer tool already uses.
#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <anno/app/ui_layout.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/types.hpp>

namespace anno::app {

/// What a construction-menu click resolves to.
enum class ConstructionClickKind {
    /// Outside every tracked rectangle (or the menu is closed and the click was not on the
    /// category column): no effect.
    None,
    /// A category-column tab: `category` names the newly open category.
    SwitchCategory,
    /// An empty grid slot (a panel-stone cell, no View.dll record): no effect.
    Empty,
    /// A locked cell (C2: the sim's construction_eligibility said not allowed): no command is
    /// sent (V1). `type` still names the id, for a UI that wants to show why.
    Locked,
    /// An unlocked cell: `type` is the Anlagen id to arm (DevTool::arm, then place as usual).
    Arm,
    /// The pickaxe (C5): toggle demolish mode.
    ToggleDemolish,
    /// The cancel-X: leave placement or demolish mode.
    Cancel,
};

struct ConstructionClickResult {
    ConstructionClickKind kind = ConstructionClickKind::None;
    /// Set for SwitchCategory, Locked and Arm.
    std::string category;
    sim::TypeId type;
};

/// The info panel's cost row (C3): the gold and build-goods costs, read live from the registry
/// (`BuildingType::cost_gold`/`cost_goods`, HEAD bytes 3-6 and the gold cost - never
/// `data/ui/construction.txt`'s own cost fields, which are RE evidence about the original's
/// panel, not the live registry a mod could change).
struct ConstructionCellInfo {
    sim::TypeId type;
    /// False when `type` is not in the registry at all (a malformed cell/registry pairing): the
    /// cost fields are then meaningless and left at their defaults.
    bool known = false;
    std::uint32_t cost_gold = 0;
    /// Ascending goods order, non-zero amounts only (BuildingType::cost_goods's own contract).
    std::vector<sim::CostEntry> cost_goods;
    /// Upkeep: nullopt (the panel should show blank, not a guessed number) until a published
    /// source exists. C3 reads "the upkeep, where the snapshot carries it; otherwise blank, with
    /// an OQ" - PlaySnapshot does not yet carry a per-type upkeep figure (OQ-563: the registry's
    /// own `running_cost` has two PROD3 slots whose "active" selection is per *instance*, not a
    /// static per-type fact a menu entry for an unbuilt placement can read before one exists).
    std::optional<std::uint32_t> upkeep;
};

class ConstructionMenu {
  public:
    /// `hud` and `construction` must outlive this object (both are the project's own long-lived
    /// data/ui/ parse results, held once per window session, same lifetime convention as the
    /// layouts themselves).
    ConstructionMenu(const HudLayout& hud, const ConstructionLayout& construction)
        : hud_(&hud), construction_(&construction) {}

    /// The layouts this menu was built from (the live window's draw path reads these too, to
    /// compose the same screens `click` resolves against - ui_mock.hpp's `draw_live_screen_xrgb`).
    [[nodiscard]] const HudLayout& hud() const { return *hud_; }
    [[nodiscard]] const ConstructionLayout& construction() const { return *construction_; }

    /// The category-column tab at (`px`,`py`), or nullopt outside every tab's band. A tab's band
    /// is its `y` field +/- half the column gadget's 46 px height (docs/re/ui.md section 2.4:
    /// "category_column (779, click_y-23) 42 x 46" - `y` is that rectangle's vertical centre).
    [[nodiscard]] std::optional<std::string> category_at(double px, double py) const;

    /// The grid cell at (`px`,`py`) of the construction grid (construction.txt's own `grid`
    /// record: origin, stride, sprite size), or nullopt outside every cell's sprite rectangle
    /// (including a stride's gutter, and outside the grid's cols x rows entirely).
    [[nodiscard]] std::optional<std::pair<int, int>> cell_at(double px, double py) const;

    /// Resolves a click at (`px`,`py`), with `open_category` the menu's currently open category
    /// ("" when the menu is closed: a grid, pickaxe or cancel-x click then resolves to None,
    /// since none of those rectangles are shown with no category open). `placement_open` is
    /// whether a cell is currently armed (the info/placement panel is showing): the pickaxe
    /// (ABRISSICON) and the cancel-X (INFOEXIT) rectangles overlap in pixels (hud.txt's measured
    /// (970,710) 53x53 and (984,725) 33x38), but docs/re/ui.md section 2.4 states the two panels
    /// are never shown together ("[cancel-X is] on the info/placement panel only; not on the
    /// category grid") - so only the panel matching `placement_open` is checked, avoiding a
    /// guess at which the player meant from pixel overlap alone. `eligibility`: the published
    /// PlaySnapshot::eligibility (ascending by id, as World::types().types() orders them) or
    /// nullptr; a cell with no matching entry, or a null `eligibility`, resolves Locked rather
    /// than guessing it open.
    [[nodiscard]] ConstructionClickResult
    click(double px, double py, const std::string& open_category, bool placement_open,
          const std::vector<std::pair<sim::TypeId, bool>>* eligibility) const;

    /// The info panel's cost row for `type` (C3), read from `registry` (the live source, not
    /// `data/ui/construction.txt`'s RE snapshot of it).
    [[nodiscard]] static ConstructionCellInfo cell_info(sim::TypeId type,
                                                        const sim::TypeRegistry& registry);

  private:
    [[nodiscard]] static bool inside(double px, double py, int x, int y, unsigned w, unsigned h);

    const HudLayout* hud_;
    const ConstructionLayout* construction_;
};

} // namespace anno::app
