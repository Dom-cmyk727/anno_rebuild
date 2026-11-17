// UI-TABS-1: the right panel's views and their sub-tabs (docs/re/ui.md section 5.4). The views,
// their sub-tab counts, frames, keys and the panel each sub-tab shows are hud.txt data (`view`,
// `subtab`, `body`, `hotkey`, `frame_idle`, `frame_selected` on the main-tab and sub-tab
// elements; data/ui/README.md); this file only holds the switching the original's code does
// (GOG View.dll / AnnoFrame.dll addresses at each function). No SDL, no drawing.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <anno/app/ui_layout.hpp>
#include <anno/formats/result.hpp>

namespace anno::app {

/// One sub-tab of a view: its hud.txt element, the `panel` group it shows (`body`) and the key
/// that shows its view at it (`hotkey`, upper case; none for most).
struct HudSubtab {
    std::string element;
    std::string body;
    std::optional<char> hotkey;
};

/// One view of the right panel: its name (`view`), the main tab that shows it, that tab's key
/// (`hotkey` on the main tab: shows the view at the sub-tab it showed last) and its sub-tabs in
/// index order (the index is the position in `subtabs`).
struct HudView {
    std::string name;
    std::string main_tab;
    std::optional<char> hotkey;
    std::vector<HudSubtab> subtabs;
};

/// The views of `hud`, in the file order of their main tabs (the first is the start state). An
/// error names the element when a sub-tab names a view no main tab shows, a view has no sub-tab
/// or a gap or a repeat in its indices, a sub-tab lacks `body`, `frame_idle` or
/// `frame_selected`, or two elements claim the same key.
[[nodiscard]] formats::Result<std::vector<HudView>, std::string> hud_views(const HudLayout& hud);

/// Sub-tab `index` of view `view` in `hud` (the element with that `view` and `subtab`), or
/// nullptr.
[[nodiscard]] const UiElement* hud_subtab_element(const HudLayout& hud, const std::string& view,
                                                  int index);

/// The view the first main tab with a `view` shows (the start state), or empty.
[[nodiscard]] std::string hud_first_view(const HudLayout& hud);

/// The frame a sub-tab element draws: `frame_selected` when `selected`, else `frame_idle`
/// (either falls back to `tex_id`).
[[nodiscard]] std::optional<int> hud_subtab_frame(const UiElement& e, bool selected);

/// The switching state: which view shows and the sub-tab each view shows. Every view keeps its
/// own sub-tab while another view shows (the original's views are persistent children of the
/// HUD, View.dll 0x100194c0 `[hud+0x5e4][i]`, and a view shown again re-shows its column's
/// selection: player view 0x1007e9c0 message 1 -> 0x1007f600, city view 0x1006a439 ->
/// 0x1006af30; the options view leaves its last page attached, 0x100787f0). The start state is
/// the first view at sub-tab 0 and every view at sub-tab 0 (hud_idle_running, r2_tab_help,
/// r2_options; the player view's column is built with selection 0).
class HudTabs {
  public:
    /// `views` must be non-empty and every view must have a sub-tab (hud_views guarantees both).
    explicit HudTabs(std::vector<HudView> views);

    [[nodiscard]] const std::vector<HudView>& views() const { return views_; }
    /// The view showing now.
    [[nodiscard]] const HudView& view() const { return views_[shown_]; }
    /// Its sub-tab.
    [[nodiscard]] int subtab() const { return selected_[shown_]; }
    /// The sub-tab view `name` shows (or would show again); nullopt for an unknown view.
    [[nodiscard]] std::optional<int> subtab_of(const std::string& name) const;
    /// The `panel` group the showing view's sub-tab draws.
    [[nodiscard]] const std::string& body() const;

    /// A press on a main tab (or its key): view `name` at the sub-tab it showed last (View.dll
    /// 0x10034ab0 -> 0x100335d0; a press on the tab already showing re-shows the same, no
    /// change). False for an unknown view.
    bool show(const std::string& name);
    /// A press on sub-tab `index` of the showing view (handlers View.dll 0x1007f730, 0x1006b070,
    /// 0x100786a0: store the column's index, show that page, hide the others; no check against
    /// the current index, so a press on the selected sub-tab shows the same page again). False
    /// when the view has no such sub-tab.
    bool select(int index);
    /// A key (either case): a sub-tab's key shows its view at that sub-tab (AnnoFrame.dll key
    /// switch 0x1000bfc4: d, c, s -> HUD message 0x10 params 5, 7, 6 -> View.dll 0x10033485 ->
    /// player view table 0x1007ea3c); a main tab's key shows its view at the sub-tab it showed
    /// last (o -> param 9 -> 0x1003353f). False when no element has that key.
    bool key(char c);
    /// The first view at sub-tab 0: Esc on the construction grid (AnnoFrame.dll 0x1000bd43 sends
    /// the HUD message 0x10 param 2; View.dll 0x1003334c with the construction tab showing and
    /// nothing selected sends param 4, 0x100333e3; 0x10033485 shows the player view and
    /// forwards 4, which selects sub-tab 0, 0x1007e9e6).
    void overview();

  private:
    std::vector<HudView> views_;
    std::vector<int> selected_;
    std::size_t shown_ = 0;
};

} // namespace anno::app
