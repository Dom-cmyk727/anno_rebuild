// UI-TABS-1: the right panel's views and sub-tabs (hud_tabs.hpp; docs/re/ui.md section 5.4). The
// real data/ui/hud.txt is the project's own derived-fact file, so these cases need no game files.
#include <filesystem>
#include <string>

#include <doctest/doctest.h>

#include <anno/app/hud_tabs.hpp>
#include <anno/app/ui_layout.hpp>
#include <anno/app/ui_mock.hpp>

namespace fs = std::filesystem;
namespace app = anno::app;

namespace {

app::HudLayout real_hud() {
    const auto loaded = app::load_hud_layout(fs::path(ANNO_REPO_DIR) / "data" / "ui" / "hud.txt");
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    return loaded.value();
}

app::HudTabs real_tabs() {
    auto views = app::hud_views(real_hud());
    REQUIRE_MESSAGE(views.ok(), (views.ok() ? std::string() : views.error()));
    return app::HudTabs(views.value());
}

/// Sub-tabs `count` of `view` cycled: each press selects that index and its body.
void check_every_subtab(app::HudTabs& tabs, const std::string& view, std::size_t count) {
    REQUIRE(tabs.show(view));
    REQUIRE(tabs.view().subtabs.size() == count);
    for (std::size_t k = 0; k < count; ++k) {
        CAPTURE(k);
        CHECK(tabs.select(static_cast<int>(k)));
        CHECK(tabs.view().name == view);
        CHECK(tabs.subtab() == static_cast<int>(k));
        CHECK(tabs.body() == tabs.view().subtabs[k].body);
    }
    CHECK_FALSE(tabs.select(static_cast<int>(count))); // one past the column: nothing
    CHECK(tabs.subtab() == static_cast<int>(count) - 1);
    CHECK_FALSE(tabs.select(-1));
}

std::string synthetic(const std::string& body) {
    return "anno1503-ui-hud 1\nresolution 1024 768\n" + body;
}

} // namespace

TEST_CASE("hud tabs: the real hud.txt has the three views with the ACCESS ranges' frames") {
    const app::HudLayout hud = real_hud();
    const auto views = app::hud_views(hud);
    REQUIRE_MESSAGE(views.ok(), (views.ok() ? std::string() : views.error()));
    REQUIRE(views.value().size() == 3);
    // Hand-derived from fenster.inc line 1 (GOG 2.0.0.5): VIEWINFOPIELERTAB,1184 (next name at
    // 1194: 10 frames), VIEWINFOSTADTTAB,661 (next 677: 16), OPTION_TABS,1476 (next 1486: 10);
    // two frames per tab, idle first (builders View.dll 0x1007f4bf.., 0x1006ad4c..).
    struct Expect {
        const char* view;
        const char* main_tab;
        std::size_t count;
        int base;
        const char* body0;
    };
    const Expect expect[] = {{"player", "tab_military", 5, 1184, "player"},
                             {"info", "tab_help", 8, 661, "info"},
                             {"options", "tab_options", 5, 1476, "main_menu"}};
    for (std::size_t i = 0; i < 3; ++i) {
        const app::HudView& v = views.value()[i];
        CAPTURE(v.name);
        CHECK(v.name == expect[i].view);
        CHECK(v.main_tab == expect[i].main_tab);
        REQUIRE(v.subtabs.size() == expect[i].count);
        CHECK(v.subtabs[0].body == expect[i].body0);
        for (std::size_t k = 0; k < v.subtabs.size(); ++k) {
            CAPTURE(k);
            const app::UiElement* e = hud.element(v.subtabs[k].element);
            REQUIRE(e != nullptr);
            CHECK(e == app::hud_subtab_element(hud, v.name, static_cast<int>(k)));
            CHECK(app::hud_subtab_frame(*e, false) == expect[i].base + 2 * static_cast<int>(k));
            CHECK(app::hud_subtab_frame(*e, true) == expect[i].base + 2 * static_cast<int>(k) + 1);
            CHECK(e->pos_x == 762);
            CHECK(e->pos_y == 303 + 46 * static_cast<int>(k));
        }
    }
    // The R2 panels keep their names: player tab 1 is the player list (r2_player_menu).
    CHECK(views.value()[0].subtabs[1].body == "player_list");
    CHECK(app::hud_first_view(hud) == "player");
    // The keys: D, C, S on the player view's sub-tabs 1, 2, 3 (AnnoFrame.dll 0x1000bfc4 ->
    // 0x1007ea3c); O on the options view's main tab.
    CHECK(views.value()[0].subtabs[1].hotkey == 'D');
    CHECK(views.value()[0].subtabs[2].hotkey == 'C');
    CHECK(views.value()[0].subtabs[3].hotkey == 'S');
    CHECK(views.value()[2].hotkey == 'O');
    CHECK_FALSE(views.value()[0].hotkey.has_value());
}

TEST_CASE("hud tabs: the start state is the player overview, every view at sub-tab 0") {
    const app::HudTabs tabs = real_tabs();
    CHECK(tabs.view().name == "player");
    CHECK(tabs.subtab() == 0);
    CHECK(tabs.body() == "player");
    CHECK(tabs.subtab_of("info") == 0);
    CHECK(tabs.subtab_of("options") == 0);
    CHECK_FALSE(tabs.subtab_of("nonsense").has_value());
}

TEST_CASE("hud tabs: every sub-tab of every view selects itself") {
    app::HudTabs tabs = real_tabs();
    check_every_subtab(tabs, "player", 5);
    check_every_subtab(tabs, "info", 8);
    check_every_subtab(tabs, "options", 5);
}

TEST_CASE("hud tabs: a main tab shows its view at the sub-tab it showed last") {
    app::HudTabs tabs = real_tabs();
    // The bust from the overview stays on the overview (View.dll 0x10034ab0 -> 0x100335d0 ->
    // 0x1007e9c0 message 1 -> 0x1007f600 re-shows the column's selection).
    CHECK(tabs.show("player"));
    CHECK(tabs.body() == "player");
    REQUIRE(tabs.select(3));
    CHECK(tabs.show("options"));
    REQUIRE(tabs.select(4));
    CHECK(tabs.body() == "options_speech");
    CHECK(tabs.show("info"));
    CHECK(tabs.subtab() == 0);
    REQUIRE(tabs.select(6));
    CHECK(tabs.show("player"));
    CHECK(tabs.subtab() == 3);
    CHECK(tabs.body() == "player_ships");
    CHECK(tabs.show("options")); // f15_key_O: the speech page is still showing
    CHECK(tabs.subtab() == 4);
    CHECK(tabs.show("info"));
    CHECK(tabs.subtab() == 6);
    CHECK(tabs.body() == "info_tab_6");
    CHECK_FALSE(tabs.show("nonsense"));
    CHECK(tabs.view().name == "info");
}

TEST_CASE("hud tabs: a press on the selected sub-tab changes nothing") {
    app::HudTabs tabs = real_tabs();
    REQUIRE(tabs.select(2));
    const app::HudTabs before = tabs;
    CHECK(tabs.select(2));
    CHECK(tabs.view().name == before.view().name);
    CHECK(tabs.subtab() == before.subtab());
    CHECK(tabs.body() == before.body());
}

TEST_CASE("hud tabs: keys D, C, S select player sub-tabs 1, 2, 3 from any view, key O the "
          "options view") {
    app::HudTabs tabs = real_tabs();
    REQUIRE(tabs.show("options"));
    REQUIRE(tabs.select(2));
    CHECK(tabs.key('d'));
    CHECK(tabs.view().name == "player");
    CHECK(tabs.subtab() == 1);
    CHECK(tabs.body() == "player_list");
    REQUIRE(tabs.show("info"));
    CHECK(tabs.key('S'));
    CHECK(tabs.view().name == "player");
    CHECK(tabs.subtab() == 3);
    CHECK(tabs.key('c'));
    CHECK(tabs.subtab() == 2);
    CHECK(tabs.body() == "player_cities");
    // O: the options view at its own last sub-tab (2), not at 0.
    CHECK(tabs.key('o'));
    CHECK(tabs.view().name == "options");
    CHECK(tabs.subtab() == 2);
    // A key hud.txt does not bind on a view: no change (B is the construction tab's, W unbound).
    CHECK_FALSE(tabs.key('b'));
    CHECK_FALSE(tabs.key('w'));
    CHECK(tabs.view().name == "options");
    CHECK(tabs.subtab() == 2);
}

TEST_CASE("hud tabs: the overview is the player view at sub-tab 0 whatever showed before") {
    app::HudTabs tabs = real_tabs();
    REQUIRE(tabs.key('s'));
    REQUIRE(tabs.show("info"));
    REQUIRE(tabs.select(5));
    tabs.overview();
    CHECK(tabs.view().name == "player");
    CHECK(tabs.subtab() == 0);
    CHECK(tabs.body() == "player");
    CHECK(tabs.subtab_of("info") == 5); // the other views keep theirs
}

TEST_CASE("hud tabs: a malformed view is refused with the element's name") {
    const std::string tab = "element tab_a pos 0 0 size 1 1 view a\n";
    const std::string sub = " pos 0 0 size 1 1 frame_idle 1 frame_selected 2 view a body x";
    struct Case {
        const char* what;
        std::string text;
        const char* needle;
    };
    const Case cases[] = {
        {"a gap", tab + "element s0" + sub + " subtab 0\nelement s2" + sub + " subtab 2\n",
         "no sub-tab 1"},
        {"a repeat", tab + "element s0" + sub + " subtab 0\nelement t0" + sub + " subtab 0\n",
         "repeats index 0"},
        {"no body",
         tab + "element s0 pos 0 0 size 1 1 frame_idle 1 frame_selected 2 view a subtab 0\n",
         "needs body"},
        {"no frames", tab + "element s0 pos 0 0 size 1 1 view a body x subtab 0\n", "needs body"},
        {"no main tab", "element s0" + sub + " subtab 0\n", "which no main tab shows"},
        {"no sub-tab", tab, "has no sub-tab"},
        {"a key twice",
         tab + "element s0" + sub + " subtab 0 hotkey D\nelement s1" + sub + " subtab 1 hotkey d\n",
         "key D on both"},
        {"no view at all", "element s0 pos 0 0 size 1 1\n", "no main tab names a view"},
    };
    for (const Case& c : cases) {
        CAPTURE(c.what);
        const auto hud = app::parse_hud_layout(synthetic(c.text));
        REQUIRE_MESSAGE(hud.ok(), (hud.ok() ? std::string() : hud.error()));
        const auto views = app::hud_views(hud.value());
        REQUIRE_FALSE(views.ok());
        CHECK_MESSAGE(views.error().find(c.needle) != std::string::npos, views.error());
    }
    // The control: the same pieces, complete, parse.
    const auto good = app::parse_hud_layout(
        synthetic(tab + "element s0" + sub + " subtab 0\nelement s1" + sub + " subtab 1\n"));
    REQUIRE(good.ok());
    const auto views = app::hud_views(good.value());
    REQUIRE_MESSAGE(views.ok(), (views.ok() ? std::string() : views.error()));
    CHECK(views.value()[0].subtabs.size() == 2);
}

TEST_CASE("hud tabs: the screen names map to views, sub-tabs and panels") {
    const app::HudLayout hud = real_hud();
    struct Case {
        const char* screen;
        const char* panel;
    };
    const Case cases[] = {{"hud_idle", "player"},
                          {"player_list", "player_list"},
                          {"info", "info"},
                          {"main_menu", "main_menu"},
                          {"player.3", "player_ships"},
                          {"info.7", "info_tab_7"},
                          {"options.3", "options_music"},
                          {"options.5", ""},
                          {"construction_public", ""},
                          {"nonsense.0", ""}};
    for (const Case& c : cases) {
        CAPTURE(c.screen);
        const auto spec = app::parse_screen_name(c.screen);
        REQUIRE(spec.has_value());
        CHECK(app::ui_panel_group(hud, *spec) == c.panel);
    }
    for (const char* bad : {"player.", ".3", "player.x", "player.1234", "hud"}) {
        CAPTURE(bad);
        CHECK_FALSE(app::parse_screen_name(bad).has_value());
    }
}
