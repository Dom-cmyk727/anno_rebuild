// UI.2 C1/C2/C4/C5: the construction menu's click resolution, against the project's own real
// data/ui/hud.txt and data/ui/construction.txt (no game files needed - these are the project's
// own checked-in derived-fact files, like ui_layout_test.cpp).
#include <algorithm>
#include <filesystem>

#include <doctest/doctest.h>

#include <anno/app/construction_menu.hpp>
#include <anno/assets/text.hpp>
#include <anno/formats/zei.hpp>
#include <anno/test/game_dir.hpp>

namespace fs = std::filesystem;
namespace app = anno::app;

namespace {
fs::path data_ui_dir() {
    return fs::path(ANNO_REPO_DIR) / "data" / "ui";
}

struct Layouts {
    app::HudLayout hud;
    app::ConstructionLayout construction;
};

Layouts load() {
    const auto hud = app::load_hud_layout(data_ui_dir() / "hud.txt");
    const auto con = app::load_construction_layout(data_ui_dir() / "construction.txt");
    REQUIRE_MESSAGE(hud.ok(), (hud.ok() ? std::string() : hud.error()));
    REQUIRE_MESSAGE(con.ok(), (con.ok() ? std::string() : con.error()));
    return {hud.value(), con.value()};
}
} // namespace

TEST_CASE("construction menu: category_at finds each of the eight column bands, and nothing "
          "outside them") {
    const Layouts l = load();
    const app::ConstructionMenu menu(l.hud, l.construction);

    // Every category's own y (docs/re/ui.md section 3.1's table) is inside its own band.
    for (const auto& cat : l.construction.categories) {
        CAPTURE(cat.name);
        const auto hit = menu.category_at(790.0, static_cast<double>(cat.y));
        REQUIRE(hit.has_value());
        CHECK(*hit == cat.name);
    }
    // public's own y (325) centre hits public specifically.
    CHECK(menu.category_at(790.0, 325.0) == std::optional<std::string>("public"));
    // Left of the column (x < 779): no hit, even at a valid y.
    CHECK_FALSE(menu.category_at(700.0, 325.0).has_value());
    // Right of the 42 px gadget width (x >= 779 + 42): no hit.
    CHECK_FALSE(menu.category_at(825.0, 325.0).has_value());
    // Above the first band (public's band starts at 325 - 23 = 302): no hit.
    CHECK_FALSE(menu.category_at(790.0, 100.0).has_value());
}

TEST_CASE("construction menu: cell_at finds the grid's sprites, not the gutters, not outside "
          "the 4 x 8 grid") {
    const Layouts l = load();
    const app::ConstructionMenu menu(l.hud, l.construction);
    const auto& g = l.construction.grid;
    REQUIRE(g.origin_x == 808);
    REQUIRE(g.origin_y == 305);
    REQUIRE(g.cell_px == 52);
    REQUIRE(g.sprite == 50);

    // The centre of cell (0, 0) hits (0, 0).
    CHECK(menu.cell_at(808.0 + 25.0, 305.0 + 25.0) == std::make_pair(0, 0));
    // The centre of cell (3, 7) (the grid's far corner, 4 cols x 8 rows) hits (3, 7).
    CHECK(menu.cell_at(808.0 + 3 * 52.0 + 25.0, 305.0 + 7 * 52.0 + 25.0) == std::make_pair(3, 7));
    // Just inside the top-left corner of cell (1, 0)'s sprite.
    CHECK(menu.cell_at(808.0 + 52.0, 305.0) == std::make_pair(1, 0));
    // The 2 px gutter between cell (0, 0) and (1, 0) (stride 52, sprite 50: x in [50, 52)): no
    // cell.
    CHECK_FALSE(menu.cell_at(808.0 + 50.5, 305.0 + 25.0).has_value());
    // Before the grid's origin: no cell.
    CHECK_FALSE(menu.cell_at(800.0, 305.0).has_value());
    // Past the 4th column / 8th row: no cell.
    CHECK_FALSE(menu.cell_at(808.0 + 4 * 52.0 + 25.0, 305.0 + 25.0).has_value());
    CHECK_FALSE(menu.cell_at(808.0 + 25.0, 305.0 + 8 * 52.0 + 25.0).has_value());
}

TEST_CASE("construction menu: click resolves a category tab, then, with a category open, an "
          "empty/locked/armable cell, the pickaxe and cancel-X - never guessing eligibility open") {
    const Layouts l = load();
    const app::ConstructionMenu menu(l.hud, l.construction);

    // The menu is closed (open_category ""): a category-tab click still switches; a grid/pickaxe/
    // cancel-X click does nothing (those rectangles are not shown with no category open).
    {
        const auto r = menu.click(790.0, 325.0, "", false, nullptr);
        CHECK(r.kind == app::ConstructionClickKind::SwitchCategory);
        CHECK(r.category == "public");
    }
    {
        const auto r = menu.click(808.0 + 25.0, 305.0 + 25.0, "", false, nullptr);
        CHECK(r.kind == app::ConstructionClickKind::None);
    }

    // public (1, 0) is id 580 (locked at the Citizen start per construction.txt - but this test
    // never reads that field; it is reached only through the `eligibility` argument, exactly as
    // the sim's live construction_eligibility would be published).
    const auto* locked_cell = l.construction.cell("public", 1, 0);
    REQUIRE(locked_cell != nullptr);
    const auto [lx, ly] = *l.construction.grid.click_centre(1, 0);

    // No eligibility published at all: Locked, never guessed open.
    {
        const auto r =
            menu.click(static_cast<double>(lx), static_cast<double>(ly), "public", false, nullptr);
        CHECK(r.kind == app::ConstructionClickKind::Locked);
        CHECK(r.type.v == locked_cell->id);
    }
    // Published, but this id is absent from the list (a malformed/partial eligibility): Locked.
    {
        const std::vector<std::pair<anno::sim::TypeId, bool>> partial{};
        const auto r =
            menu.click(static_cast<double>(lx), static_cast<double>(ly), "public", false, &partial);
        CHECK(r.kind == app::ConstructionClickKind::Locked);
    }
    // Published and false: Locked.
    {
        const std::vector<std::pair<anno::sim::TypeId, bool>> elig{
            {anno::sim::TypeId{locked_cell->id}, false}};
        const auto r =
            menu.click(static_cast<double>(lx), static_cast<double>(ly), "public", false, &elig);
        CHECK(r.kind == app::ConstructionClickKind::Locked);
    }
    // Published and true: Arm, with the cell's own type id.
    {
        const std::vector<std::pair<anno::sim::TypeId, bool>> elig{
            {anno::sim::TypeId{locked_cell->id}, true}};
        const auto r =
            menu.click(static_cast<double>(lx), static_cast<double>(ly), "public", false, &elig);
        CHECK(r.kind == app::ConstructionClickKind::Arm);
        CHECK(r.type.v == locked_cell->id);
        CHECK(r.category == "public");
    }
    // An empty slot (public row 7, cols 1-3 are "-" per docs/re/ui.md section 3.3): Empty,
    // regardless of eligibility.
    {
        REQUIRE(l.construction.cell("public", 1, 7) == nullptr);
        const auto [ex, ey] = *l.construction.grid.click_centre(1, 7);
        const auto r =
            menu.click(static_cast<double>(ex), static_cast<double>(ey), "public", false, nullptr);
        CHECK(r.kind == app::ConstructionClickKind::Empty);
    }

    // The pickaxe (data/ui/hud.txt "abrissicon", (970, 710) 53 x 53), on the plain category grid
    // (placement_open false).
    {
        const auto r = menu.click(970.0 + 10.0, 710.0 + 10.0, "public", false, nullptr);
        CHECK(r.kind == app::ConstructionClickKind::ToggleDemolish);
    }
    // Cancel-X (data/ui/hud.txt "infoexit", (984, 725) 33 x 35), on the info/placement panel
    // (placement_open true) - the same point resolves ToggleDemolish instead when false, since
    // the two rectangles overlap and only the state tells them apart (see the header comment).
    {
        const auto r = menu.click(984.0 + 5.0, 725.0 + 5.0, "public", true, nullptr);
        CHECK(r.kind == app::ConstructionClickKind::Cancel);
    }
    {
        const auto r = menu.click(984.0 + 5.0, 725.0 + 5.0, "public", false, nullptr);
        CHECK(r.kind == app::ConstructionClickKind::ToggleDemolish);
    }
    // The info/placement panel covers the grid's own extent while it is open (data/ui/hud.txt
    // "info_panel", (804, 302) 220 x 466). A click on it away from infoexit must be a no-op, not
    // a fall-through into cell_at() against grid coordinates the panel is drawn over: the same
    // point that resolves Arm with the menu open (placement_open false) must resolve None with
    // the panel open instead, never silently re-arming a different type.
    {
        const std::vector<std::pair<anno::sim::TypeId, bool>> elig{
            {anno::sim::TypeId{locked_cell->id}, true}};
        const auto r =
            menu.click(static_cast<double>(lx), static_cast<double>(ly), "public", true, &elig);
        CHECK(r.kind == app::ConstructionClickKind::None);
    }
    // Outside every tracked rectangle: None.
    {
        const auto r = menu.click(10.0, 10.0, "public", false, nullptr);
        CHECK(r.kind == app::ConstructionClickKind::None);
    }
}

TEST_CASE("construction menu: cell_info reads cost and upkeep live from the registry "
          "(UI.2 C3, UI-INFO-1)") {
    anno::sim::TypeRegistry::Builder b;
    anno::sim::BuildingType priced;
    priced.id = anno::sim::TypeId{501};
    priced.cost_gold = 250;
    priced.cost_goods = {{41, 300}, {42, 700}}; // tools, wood (8.8 fixed-point, as the registry
                                                // stores every cost amount)
    b.add(priced);
    // UI-INFO-1: the panel's upkeep is PROD3's active column, running_cost[1] (docs/re/ui.md
    // section 2.5; the forester 801's own PROD3 is {4, 12} and the GOG panel shows 12).
    anno::sim::BuildingType running;
    running.id = anno::sim::TypeId{801};
    running.cost_gold = 150;
    running.running_cost = {4, 12};
    b.add(running);
    const auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));

    const auto known = app::ConstructionMenu::cell_info(anno::sim::TypeId{501}, *built.value());
    CHECK(known.known);
    CHECK(known.cost_gold == 250);
    REQUIRE(known.cost_goods.size() == 2);
    CHECK(known.cost_goods[0].goods == 41);
    CHECK(known.cost_goods[0].amount_88 == 300);
    CHECK(known.cost_goods[1].goods == 42);
    CHECK(known.cost_goods[1].amount_88 == 700);
    // A type without the two-column running cost: no upkeep, never a guessed one.
    CHECK_FALSE(known.upkeep.has_value());

    const auto active = app::ConstructionMenu::cell_info(anno::sim::TypeId{801}, *built.value());
    REQUIRE(active.upkeep.has_value());
    CHECK(*active.upkeep == 12); // column 1 (active), not column 0 (4)

    const auto unknown = app::ConstructionMenu::cell_info(anno::sim::TypeId{9999}, *built.value());
    CHECK_FALSE(unknown.known);
    CHECK(unknown.cost_gold == 0);
    CHECK(unknown.cost_goods.empty());
    CHECK_FALSE(unknown.upkeep.has_value());
}

TEST_CASE("construction menu: info_panel_content - whole tons by goods name, gold, upkeep "
          "(UI-INFO-1)") {
    anno::sim::TypeRegistry::Builder b;
    anno::sim::BuildingType kontor;
    kontor.id = anno::sim::TypeId{1302};
    kontor.cost_gold = 350;
    const auto& reg0 = *anno::sim::TypeRegistry::empty();
    const auto tools = reg0.goods_by_name("WERKZEUG");
    const auto wood = reg0.goods_by_name("HOLZ");
    REQUIRE(tools.has_value());
    REQUIRE(wood.has_value());
    // 5 t tools, 12 t wood and a fraction that the original's `>> 8` drops (View.dll
    // 0x100301e2): 12 t + 255/256 still shows 12.
    kontor.cost_goods = {{*tools, 5 * 256}, {*wood, 12 * 256 + 255}};
    std::sort(kontor.cost_goods.begin(), kontor.cost_goods.end(),
              [](const auto& a, const auto& c) { return a.goods < c.goods; });
    kontor.running_cost = {0, 15};
    b.add(kontor);
    const auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));

    const auto info =
        app::ConstructionMenu::info_panel_content(anno::sim::TypeId{1302}, *built.value());
    REQUIRE(info.has_value());
    CHECK(info->type == anno::sim::TypeId{1302});
    CHECK(info->tools == 5);
    CHECK(info->wood == 12);
    CHECK(info->bricks == 0);
    CHECK(info->marble == 0);
    CHECK(info->gold == 350);
    REQUIRE(info->upkeep.has_value());
    CHECK(*info->upkeep == 15);
    // Fix round F, F5: a type without the two-column running cost has no upkeep source - the
    // panel shows nothing there, never a guessed 0.
    anno::sim::TypeRegistry::Builder b2;
    anno::sim::BuildingType bare;
    bare.id = anno::sim::TypeId{501};
    b2.add(bare);
    const auto built2 = b2.build();
    REQUIRE_MESSAGE(built2.ok(), (built2.ok() ? std::string() : built2.error()));
    const auto bare_info =
        app::ConstructionMenu::info_panel_content(anno::sim::TypeId{501}, *built2.value());
    REQUIRE(bare_info.has_value());
    CHECK_FALSE(bare_info->upkeep.has_value());
    CHECK_FALSE(app::ConstructionMenu::info_panel_content(anno::sim::TypeId{9999}, *built.value())
                    .has_value());
}

TEST_CASE("info panel: centred_text_x, the original's centring rule (UI-INFO-1)") {
    // x = box_x + box_w / 2 - text_w / 2, both halves rounding down. The figures are the GOG
    // measurements (docs/re/ui.md section 2.5): MainSmall advance widths of 1-, 2- and 3-digit
    // values in the gold box (852, 145) and the material box (852, 50), and two names in the
    // name box (819, 200).
    CHECK(app::centred_text_x(852, 145, 7) == 921);   // "5"
    CHECK(app::centred_text_x(852, 145, 9) == 920);   // "0"
    CHECK(app::centred_text_x(852, 145, 16) == 916);  // "12"
    CHECK(app::centred_text_x(852, 145, 17) == 916);  // "10"
    CHECK(app::centred_text_x(852, 145, 24) == 912);  // "150"
    CHECK(app::centred_text_x(852, 145, 25) == 912);  // "220"
    CHECK(app::centred_text_x(852, 145, 28) == 910);  // "140"
    CHECK(app::centred_text_x(852, 50, 7) == 874);    // "5"
    CHECK(app::centred_text_x(852, 50, 8) == 873);    // "2"
    CHECK(app::centred_text_x(954, 50, 16) == 971);   // "12"
    CHECK(app::centred_text_x(819, 200, 107) == 866); // the pioneer house's name
    CHECK(app::centred_text_x(819, 200, 166) == 836); // the market's name
    // Wider than the box: still the same rule, never clamped.
    CHECK(app::centred_text_x(852, 50, 61) == 847);
}

TEST_CASE("info panel: the real hud.txt text boxes place real MainSmall digits where the GOG "
          "shots show them (UI-INFO-1)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto layouts = load();
    const auto bytes = anno::test::read_file(*dir / "ToolGfx" / "MainSmall.zei");
    const auto font = anno::formats::zei::CharSet::parse(bytes);
    REQUIRE(font.ok());
    struct Case {
        const char* box;
        const char* text;
        int x, y;
    };
    // Glyph-match positions on r2_info_forester_801 / f12_info_kontor_1302 /
    // r2_info_dirt_road (1-, 2- and 3-digit values; docs/re/ui.md section 2.5).
    const Case cases[] = {
        {"info_gold", "150", 912, 624},  {"info_gold", "5", 921, 624},
        {"info_upkeep", "12", 916, 684}, {"info_upkeep", "0", 920, 684},
        {"info_tools", "2", 873, 564},   {"info_wood", "12", 971, 564},
        {"info_marble", "0", 975, 593},  {"info_bricks", "0", 873, 593},
    };
    for (const auto& c : cases) {
        const auto* el = layouts.hud.element(c.box);
        REQUIRE(el != nullptr);
        CHECK(el->align == "centre");
        const auto image = anno::assets::render_text(font.value(), c.text);
        CHECK_MESSAGE(app::centred_text_x(el->pos_x, el->size_w, image.width) == c.x, c.box);
        CHECK_MESSAGE(el->pos_y == c.y, c.box);
    }
}
