// UI.2 R1: parsing data/ui/hud.txt and data/ui/construction.txt (ADR 0004 "Widgets" - no
// coordinate lives in code). These are the project's own checked-in derived-fact files, not
// original assets, so these cases need no ANNO1503_GAME_DIR and run everywhere.
#include <filesystem>

#include <doctest/doctest.h>

#include <anno/app/ui_layout.hpp>

namespace fs = std::filesystem;

namespace {
fs::path data_ui_dir() {
    return fs::path(ANNO_REPO_DIR) / "data" / "ui";
}
} // namespace

TEST_CASE("hud layout: parses the real data/ui/hud.txt") {
    const auto loaded = anno::app::load_hud_layout(data_ui_dir() / "hud.txt");
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    const auto& hud = loaded.value();
    CHECK(hud.resolution_w == 1024);
    CHECK(hud.resolution_h == 768);
    CHECK(hud.shot == "hud_idle_running");

    const auto* statusbar = hud.element("statusbar");
    REQUIRE(statusbar != nullptr);
    CHECK(statusbar->has_pos);
    CHECK(statusbar->pos_x == 0);
    CHECK(statusbar->pos_y == 733);
    CHECK(statusbar->has_size);
    CHECK(statusbar->size_w == 746);
    CHECK(statusbar->size_h == 35);
    CHECK(statusbar->file == "toolgfx/statusbar.tex");
    CHECK(statusbar->access == "STATUSBAR");
    REQUIRE(statusbar->tex_id.has_value());
    CHECK(*statusbar->tex_id == 0);
    CHECK(statusbar->method == "sprite-sad");

    // tab_construction carries frame_idle/frame_selected and a hotkey (docs/re/ui.md section 2.1).
    const auto* tab = hud.element("tab_construction");
    REQUIRE(tab != nullptr);
    REQUIRE(tab->frame_idle.has_value());
    CHECK(*tab->frame_idle == 8);
    REQUIRE(tab->frame_selected.has_value());
    CHECK(*tab->frame_selected == 12);
    REQUIRE(tab->hotkey.has_value());
    CHECK(*tab->hotkey == 'B');

    // A baumat element carries a segment index (docs/re/ui.md section 2.3).
    const auto* tools = hud.element("baumat_tools");
    REQUIRE(tools != nullptr);
    REQUIRE(tools->segment.has_value());
    CHECK(*tools->segment == 0);

    // Elements with no measured position (inc-size / view-dll) have has_pos false.
    const auto* karteplus = hud.element("karteplus_access");
    REQUIRE(karteplus != nullptr);
    CHECK_FALSE(karteplus->has_pos);
    CHECK(karteplus->method == "inc-size");

    bool found_click = false;
    for (const auto& c : hud.clicks) {
        if (c.name == "tab_construction") {
            found_click = true;
            CHECK(c.pos_x == 809);
            CHECK(c.pos_y == 212);
        }
    }
    CHECK(found_click);
}

TEST_CASE("construction layout: parses the real data/ui/construction.txt") {
    const auto loaded = anno::app::load_construction_layout(data_ui_dir() / "construction.txt");
    REQUIRE(loaded.ok());
    const auto& c = loaded.value();
    CHECK(c.resolution_w == 1024);
    CHECK(c.resolution_h == 768);
    REQUIRE(c.open_key.has_value());
    CHECK(*c.open_key == 'B');
    CHECK(c.leave_key == "ESC");

    CHECK(c.grid.cols == 4);
    CHECK(c.grid.rows == 8);
    CHECK(c.grid.cell_px == 52);
    CHECK(c.grid.origin_x == 808);
    CHECK(c.grid.origin_y == 305);
    CHECK(c.grid.sprite == 50);
    REQUIRE(c.grid.click_x.size() == 4);
    REQUIRE(c.grid.click_y.size() == 8);
    const auto centre = c.grid.click_centre(0, 0);
    REQUIRE(centre.has_value());
    CHECK(centre->first == 833);
    CHECK(centre->second == 330);
    CHECK_FALSE(c.grid.click_centre(4, 0).has_value());

    // Eight categories, docs/re/ui.md section 3.1's table.
    REQUIRE(c.categories.size() == 8);
    const auto* public_cat = c.category("public");
    REQUIRE(public_cat != nullptr);
    CHECK(public_cat->index == 1);
    CHECK(public_cat->y == 325);
    CHECK(public_cat->tooltip_section == "TOOLTIP:BAUMENU_DIVERSES");
    CHECK(public_cat->thumbnail == "THUMBNAILDIVERSES");
    CHECK(public_cat->thumb_tex == 154);
    CHECK(public_cat->table == "0x100a7330");
    CHECK(public_cat->records == 32);
    CHECK(c.category("nonexistent") == nullptr);

    // 148 non-empty cells total (docs/re/ui.md section 3.3: "Typed non-empty cells: 148").
    CHECK(c.cells.size() == 148);

    const auto* pioneer = c.cell("public", 0, 0);
    REQUIRE(pioneer != nullptr);
    CHECK(pioneer->id == 501);
    CHECK(pioneer->unlocked);
    CHECK(pioneer->inframin == "INFRA_EVER");
    CHECK(pioneer->gold == 0);
    CHECK(pioneer->wood == 3);
    CHECK(pioneer->source == "view_dll");
    CHECK(pioneer->shot == "r2_info_pioneer_house");

    const auto* locked = c.cell("public", 1, 0);
    REQUIRE(locked != nullptr);
    CHECK(locked->id == 580);
    CHECK_FALSE(locked->unlocked);
    CHECK(locked->inframin == "INFRA_STUFE_4F");
    CHECK(locked->shot.empty()); // "-" parses to the absent/empty string

    CHECK(c.cell("public", 1, 7) == nullptr); // an empty (id 1) record is not in the list

    CHECK(c.demolish.pos_x == 993);
    CHECK(c.demolish.pos_y == 714);
    CHECK(c.demolish.shot == "r2_demolish");
    CHECK(c.demolish.access == "ABRISSICON");

    // Exactly 15 unlocked cells, 133 locked (docs/re/ui.md section 3.3: "148 (U=15, L=133)").
    std::size_t unlocked_count = 0;
    for (const auto& cell : c.cells)
        unlocked_count += cell.unlocked ? 1 : 0;
    CHECK(unlocked_count == 15);
    CHECK(c.cells.size() - unlocked_count == 133);
}

TEST_CASE("mask layout: parses the real data/ui/masks.txt, including the live-digit rectangles "
          "draw_live_screen_xrgb/_rgba read (H2)") {
    const auto loaded = anno::app::load_mask_layout(data_ui_dir() / "masks.txt");
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    const auto& m = loaded.value();
    CHECK(m.resolution_w == 1024);
    CHECK(m.resolution_h == 768);

    const auto* gold = m.mask("gold_digits");
    REQUIRE(gold != nullptr);
    CHECK(gold->pos_x == 34);
    CHECK(gold->pos_y == 733);
    CHECK(gold->size_w == 96);
    CHECK(gold->size_h == 35);
    CHECK(gold->reason == "live-digits");

    const auto* tools = m.mask("baumat_tools_d");
    REQUIRE(tools != nullptr);
    CHECK(tools->pos_x == 162);
    const auto* wood = m.mask("baumat_wood_d");
    REQUIRE(wood != nullptr);
    CHECK(wood->pos_x == 222);
    const auto* bricks = m.mask("baumat_bricks_d");
    REQUIRE(bricks != nullptr);
    CHECK(bricks->pos_x == 282);
    const auto* marble = m.mask("baumat_marble_d");
    REQUIRE(marble != nullptr);
    CHECK(marble->pos_x == 342);

    const auto* residents = m.mask("einwohner_digits");
    REQUIRE(residents != nullptr);
    CHECK(residents->pos_x == 439);
    CHECK(residents->pos_y == 733);
    CHECK(residents->size_w == 108);
    CHECK(residents->size_h == 35);

    CHECK(m.mask("no such mask") == nullptr);
}

TEST_CASE("ui layout: parse errors name the line and what was wrong") {
    const auto no_version = anno::app::parse_hud_layout("resolution 1024 768\n");
    REQUIRE_FALSE(no_version.ok());
    CHECK(no_version.error().find("line 1") != std::string::npos);

    const auto bad_pos = anno::app::parse_hud_layout("anno1503-ui-hud 1\n"
                                                     "element statusbar pos x 733 size 746 35\n");
    REQUIRE_FALSE(bad_pos.ok());
    CHECK(bad_pos.error().find("line 2") != std::string::npos);
    CHECK(bad_pos.error().find("pos") != std::string::npos);

    const auto unknown_kind =
        anno::app::parse_construction_layout("anno1503-ui-construction 1\nbogus 1 2 3\n");
    REQUIRE_FALSE(unknown_kind.ok());
    CHECK(unknown_kind.error().find("unknown line kind") != std::string::npos);

    const auto bad_state = anno::app::parse_construction_layout(
        "anno1503-ui-construction 1\ncell public 0 0 id 501 state maybe\n");
    REQUIRE_FALSE(bad_state.ok());
    CHECK(bad_state.error().find("state") != std::string::npos);

    const auto bad_mask =
        anno::app::parse_mask_layout("anno1503-ui-masks 1\nmask gold_digits pos 34 size 96 35\n");
    REQUIRE_FALSE(bad_mask.ok());
    CHECK(bad_mask.error().find("line 2") != std::string::npos);
    CHECK(bad_mask.error().find("pos") != std::string::npos);
}
