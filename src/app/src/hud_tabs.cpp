// UI-TABS-1: the right panel's views and sub-tabs (hud_tabs.hpp; docs/re/ui.md section 5.4).
#include <cctype>
#include <map>
#include <utility>

#include <anno/app/hud_tabs.hpp>

namespace anno::app {

namespace {

char upper(char c) {
    return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
}

} // namespace

formats::Result<std::vector<HudView>, std::string> hud_views(const HudLayout& hud) {
    std::vector<HudView> views;
    for (const UiElement& e : hud.elements) {
        if (e.view.empty() || e.subtab)
            continue;
        for (const HudView& v : views)
            if (v.name == e.view)
                return std::string("hud.txt: view '" + e.view + "' has two main tabs (" +
                                   v.main_tab + ", " + e.name + ")");
        HudView v;
        v.name = e.view;
        v.main_tab = e.name;
        if (e.hotkey)
            v.hotkey = upper(*e.hotkey);
        views.push_back(std::move(v));
    }
    // Sub-tabs by view and index (ordered containers: the index order is the column order).
    std::map<std::string, std::map<int, const UiElement*>> by_view;
    for (const UiElement& e : hud.elements) {
        if (!e.subtab)
            continue;
        if (e.view.empty())
            return std::string("hud.txt: sub-tab " + e.name + " names no view");
        if (e.body.empty() || !e.frame_idle || !e.frame_selected)
            return std::string("hud.txt: sub-tab " + e.name +
                               " needs body, frame_idle and frame_selected");
        if (!by_view[e.view].emplace(*e.subtab, &e).second)
            return std::string("hud.txt: sub-tab " + e.name + " repeats index " +
                               std::to_string(*e.subtab) + " of view " + e.view);
    }
    std::map<char, std::string> keys;
    const auto claim = [&keys](char key, const std::string& name) -> std::optional<std::string> {
        const auto [it, fresh] = keys.emplace(key, name);
        if (!fresh)
            return "hud.txt: key " + std::string(1, key) + " on both " + it->second + " and " +
                   name;
        return std::nullopt;
    };
    for (HudView& v : views) {
        if (v.hotkey)
            if (auto err = claim(*v.hotkey, v.main_tab))
                return *err;
        const auto found = by_view.find(v.name);
        if (found == by_view.end())
            return std::string("hud.txt: view '" + v.name + "' has no sub-tab");
        int next = 0;
        for (const auto& [index, e] : found->second) {
            if (index != next)
                return std::string("hud.txt: view '" + v.name + "' has no sub-tab " +
                                   std::to_string(next));
            ++next;
            HudSubtab s;
            s.element = e->name;
            s.body = e->body;
            if (e->hotkey) {
                s.hotkey = upper(*e->hotkey);
                if (auto err = claim(*s.hotkey, e->name))
                    return *err;
            }
            v.subtabs.push_back(std::move(s));
        }
        by_view.erase(found);
    }
    if (!by_view.empty())
        return std::string("hud.txt: sub-tabs of view '" + by_view.begin()->first +
                           "', which no main tab shows");
    if (views.empty())
        return std::string("hud.txt: no main tab names a view");
    return views;
}

const UiElement* hud_subtab_element(const HudLayout& hud, const std::string& view, int index) {
    for (const UiElement& e : hud.elements)
        if (e.subtab && *e.subtab == index && e.view == view)
            return &e;
    return nullptr;
}

std::string hud_first_view(const HudLayout& hud) {
    for (const UiElement& e : hud.elements)
        if (!e.view.empty() && !e.subtab)
            return e.view;
    return {};
}

std::optional<int> hud_subtab_frame(const UiElement& e, bool selected) {
    const std::optional<int> frame = selected ? e.frame_selected : e.frame_idle;
    return frame ? frame : e.tex_id;
}

HudTabs::HudTabs(std::vector<HudView> views)
    : views_(std::move(views)), selected_(views_.size(), 0) {}

std::optional<int> HudTabs::subtab_of(const std::string& name) const {
    for (std::size_t i = 0; i < views_.size(); ++i)
        if (views_[i].name == name)
            return selected_[i];
    return std::nullopt;
}

const std::string& HudTabs::body() const {
    return views_[shown_].subtabs[static_cast<std::size_t>(selected_[shown_])].body;
}

bool HudTabs::show(const std::string& name) {
    for (std::size_t i = 0; i < views_.size(); ++i) {
        if (views_[i].name == name) {
            shown_ = i;
            return true;
        }
    }
    return false;
}

bool HudTabs::select(int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= views_[shown_].subtabs.size())
        return false;
    selected_[shown_] = index;
    return true;
}

bool HudTabs::key(char c) {
    const char k = upper(c);
    for (std::size_t i = 0; i < views_.size(); ++i) {
        if (views_[i].hotkey == k) {
            shown_ = i;
            return true;
        }
        for (std::size_t s = 0; s < views_[i].subtabs.size(); ++s) {
            if (views_[i].subtabs[s].hotkey == k) {
                shown_ = i;
                selected_[i] = static_cast<int>(s);
                return true;
            }
        }
    }
    return false;
}

void HudTabs::overview() {
    shown_ = 0;
    selected_[0] = 0;
}

} // namespace anno::app
